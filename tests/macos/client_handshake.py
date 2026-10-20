#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the native macOS client after Cocoa has started, using isolated mTLS."""
import hashlib
import pathlib
import socket
import ssl
import struct
import subprocess
import sys
import tempfile
import time


def receive_exact(stream, size):
    result = b""
    while len(result) < size:
        chunk = stream.recv(size - len(result))
        if not chunk:
            raise RuntimeError("Client closed before replying to the protocol hello")
        result += chunk
    return result


def main():
    executable, openssl = sys.argv[1:3]
    with tempfile.TemporaryDirectory(prefix="inputserver-handshake-") as directory:
        root = pathlib.Path(directory)
        profile = root / "profile"
        (profile / "SSL/Fingerprints").mkdir(parents=True)
        key, cert = root / "key.pem", root / "cert.pem"
        subprocess.run([openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                        "-keyout", str(key), "-out", str(cert), "-days", "1",
                        "-subj", "/CN=InputServerTest"], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=15)
        pem = profile / "SSL/InputServer.pem"
        pem.write_bytes(key.read_bytes() + cert.read_bytes())
        pem.chmod(0o600)
        fingerprint = hashlib.sha256(ssl.PEM_cert_to_DER_cert(cert.read_text())).hexdigest()
        (profile / "SSL/Fingerprints/TrustedServers.txt").write_text(
            "v2:sha256:" + fingerprint + "\n")
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(str(pem))
        context.verify_mode = ssl.CERT_REQUIRED
        context.load_verify_locations(str(cert))
        # The test pins a self-signed leaf rather than a certificate authority.
        context.verify_flags &= ~ssl.VERIFY_X509_STRICT
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            listener.settimeout(8)
            with (root / "client.log").open("w+") as log:
                process = subprocess.Popen([
                    executable, "-f", "--no-tray", "--debug", "INFO",
                    "--enable-drag-drop", "--name", "test-client",
                    "--profile-dir", str(profile),
                    "127.0.0.1:" + str(listener.getsockname()[1])], stdout=log, stderr=subprocess.STDOUT)
                try:
                    raw, _ = listener.accept()
                    with raw:
                        raw.settimeout(5)
                        with context.wrap_socket(raw, server_side=True) as stream:
                            # The old client could process hello only before NSApplication
                            # started consuming its native queue. Send it after startup.
                            time.sleep(0.5)
                            hello = b"InputServer" + struct.pack("!hh", 1, 6)
                            stream.sendall(struct.pack("!I", len(hello)) + hello)
                            size = struct.unpack("!I", receive_exact(stream, 4))[0]
                            if size > 1024:
                                raise RuntimeError("Unexpected hello reply size")
                            reply = receive_exact(stream, size)
                            expected = hello + struct.pack("!I", len(b"test-client")) + b"test-client"
                            if reply != expected:
                                raise RuntimeError("Incorrect protocol hello reply")
                except Exception:
                    log.flush()
                    log.seek(0)
                    print(log.read(), file=sys.stderr)
                    raise
                finally:
                    process.terminate()
                    try:
                        process.wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=2)
    print("Client processed the delayed protocol hello after Cocoa startup")


if __name__ == "__main__":
    main()
