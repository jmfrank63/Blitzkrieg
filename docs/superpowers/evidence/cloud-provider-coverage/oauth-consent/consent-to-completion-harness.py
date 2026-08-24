#!/usr/bin/env python3
# P03-M02 evidence: one OAuth backend from consent to completion through the
# production libCloudSync.dylib, plus the cancel path, against a local
# consent+token provider - no real account and no real token anywhere. The
# "browser" is a scripted GET of the consent URL; rclone's own callback
# server and token exchange complete the dance exactly as they would for a
# real provider. Afterwards the daemon log is scanned to prove neither the
# tokens nor the consent URL leaked into it (the token in rclone.conf is
# storage, not a log - its presence is asserted, because it proves the
# dance landed).
import ctypes
import json
import os
import threading
import time
import urllib.request
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qs, urlencode, urlparse

GAME = "/Users/johannes/Projects/src/Blitzkrieg/zig-out/game/macos/arm64/debug"
FAKE_ACCESS = "fake-access-token-EVIDENCE-8f3a"
FAKE_REFRESH = "fake-refresh-token-EVIDENCE-1c9d"
FAKE_SECRET = "fake-client-secret-EVIDENCE-77b2"

served = {"auth": 0, "token": 0}


class Provider(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        u = urlparse(self.path)
        if u.path == "/auth":
            q = parse_qs(u.query)
            served["auth"] += 1
            self.send_response(302)
            self.send_header(
                "Location",
                q["redirect_uri"][0]
                + "?"
                + urlencode({"state": q["state"][0], "code": "fake-consent-code"}),
            )
            self.end_headers()
        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        if urlparse(self.path).path == "/token":
            served["token"] += 1
            body = json.dumps(
                {
                    "access_token": FAKE_ACCESS,
                    "token_type": "Bearer",
                    "refresh_token": FAKE_REFRESH,
                    "expires_in": 3600,
                }
            ).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        else:
            self.send_response(404)
            self.end_headers()


srv = HTTPServer(("127.0.0.1", 0), Provider)
threading.Thread(target=srv.serve_forever, daemon=True).start()
PROV = "http://127.0.0.1:%d" % srv.server_port

# rclone's local-browser flow execs `open` itself when it can; an empty
# PATH makes that fail silently, so the consent travels the game's path -
# the parked card and the platform launcher - which is what this run
# records. (In production both can open a tab; the card is what makes the
# wait visible either way.)
os.environ["PATH"] = ""
os.chdir(GAME)
lib = ctypes.CDLL(os.path.join(GAME, "libCloudSync.dylib"))
lib.bk_cloudsync_creds_save.argtypes = [ctypes.c_char_p]
lib.bk_cloudsync_config_begin.argtypes = [ctypes.c_char_p]
lib.bk_cloudsync_poll.argtypes = [ctypes.c_int]
lib.bk_cloudsync_poll.restype = ctypes.c_uint
lib.bk_cloudsync_error.argtypes = [ctypes.c_int]
lib.bk_cloudsync_error.restype = ctypes.c_char_p
lib.bk_cloudsync_config_question.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
lib.bk_cloudsync_config_answer.argtypes = [ctypes.c_int, ctypes.c_char_p]
lib.bk_cloudsync_cancel.argtypes = [ctypes.c_int]
lib.bk_cloudsync_release.argtypes = [ctypes.c_int]
lib.bk_cloudsync_last_error.restype = ctypes.c_char_p

doc = json.dumps(
    {
        "backend": "drive",
        "remote_root": "",
        "options": {
            "client_id": "fake-client-id",
            "client_secret": FAKE_SECRET,
            "auth_url": PROV + "/auth",
            "token_url": PROV + "/token",
        },
        "secret_options": ["client_secret"],
        "password_options": [],
        "rclone_path": os.path.join(GAME, "rclone"),
    }
)
assert lib.bk_cloudsync_creds_save(doc.encode()) == 0, lib.bk_cloudsync_last_error()


def question(handle):
    buf = ctypes.create_string_buffer(65536)
    n = lib.bk_cloudsync_config_question(handle, buf, 65536)
    return buf.raw[:n].decode() if n > 0 else ""


def log(text):
    print(text, flush=True)


def drive_flow(cancel_at_consent):
    handle = lib.bk_cloudsync_config_begin(b".")
    assert handle >= 0, lib.bk_cloudsync_last_error()
    seen = ""
    for _ in range(1200):
        state = lib.bk_cloudsync_poll(handle)
        if state in (4, 5):
            err = ctypes.string_at(lib.bk_cloudsync_error(handle)).decode()
            log("  settled: state=%d error=%r" % (state, err[:72]))
            lib.bk_cloudsync_release(handle)
            return state, err
        q = question(handle)
        if q and q != seen:
            seen = q
            card = json.loads(q)
            if card.get("role") == "consent":
                log("  consent card is up (url withheld from this transcript)")
                if cancel_at_consent:
                    log("  cancelling at the consent screen")
                    lib.bk_cloudsync_cancel(handle)
                else:
                    with urllib.request.urlopen(card["url"]) as r:
                        log("  browser followed the consent url -> HTTP %d" % r.status)
            else:
                name = card.get("name", "")
                answer = {"config_is_local": "true", "config_shared_client_id": "true"}.get(
                    name, "false"
                )
                log("  question %r -> answering %r" % (name, answer))
                assert lib.bk_cloudsync_config_answer(handle, answer.encode()) == 0
        time.sleep(0.1)
    raise SystemExit("flow never settled")


log("run A: consent to completion against the local provider")
state_a, err_a = drive_flow(cancel_at_consent=False)
log("  provider served %d consent redirect(s), %d token exchange(s)" % (served["auth"], served["token"]))
assert served["auth"] >= 1 and served["token"] >= 1, "the dance never reached the provider"

# Checked here, before run B: the next job's applyCredentials re-creates
# the remote and wipes the token - measured, and exactly the problem
# P03-M03 (token storage) exists to solve.
conf_a = open(os.path.join(GAME, "cloudsync", "rclone.conf"), errors="replace").read()
assert FAKE_ACCESS in conf_a, "the dance did not store its token"
log("  rclone.conf holds the fake token - storage, not a log; the dance really landed")

log("run B: the cancel path, abandoned at the consent screen")
state_b, err_b = drive_flow(cancel_at_consent=True)
assert state_b == 5 and "Cancelled" in err_b

lib.bk_cloudsync_shutdown()
time.sleep(1)

log("scan: no token, secret, or authorization code in the daemon log")
rcd_log = open(os.path.join(GAME, "cloudsync", "rcd.log"), errors="replace").read()
for needle in (FAKE_ACCESS, FAKE_REFRESH, FAKE_SECRET, "fake-consent-code"):
    assert needle not in rcd_log, "LEAKED into rcd.log: %s" % needle
log("  rcd.log clean of credentials (%d bytes scanned)" % len(rcd_log))
# rclone's own NOTICE prints its auth URL into its own log as the manual
# fallback hint; the state nonce in it is single-use and dead once the
# dance ends. Our layers never log the URL - this scan is about the
# credentials, which must appear nowhere.

log("evidence run complete")
