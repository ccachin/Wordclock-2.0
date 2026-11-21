
import argparse
import base64
import getpass
import json
import sys
import time

from pathlib import Path
from urllib.request import Request, urlopen
from urllib.parse import urlencode
from urllib.error import HTTPError, URLError
from http.client import HTTPException


# ======================================================
# Configuration
# ======================================================

PROJECT_DIR = Path(__file__).resolve().parent.parent

WEB_DIR = PROJECT_DIR / "data" / "www"

DEFAULT_HOST = "http://wordclock.local"

USERNAME = "admin"

FILES = [
    "style.css",
    "app.js",
    "index.html",
]

TIMEOUT = 20

READ_ATTEMPTS = 3

RETRY_DELAY = 1.0

VERIFY_DELAY = 0.5


# ======================================================
# HTTP helper
# ======================================================

def request(
    url,
    authorization=None,
    method="GET",
    data=None,
    content_type=None,
):

    headers = {
        "Cache-Control": "no-cache",
        "Connection": "close",
    }

    if authorization:
        headers["Authorization"] = authorization

    if content_type:
        headers["Content-Type"] = content_type

    attempts = READ_ATTEMPTS if method == "GET" else 1

    for attempt in range(1, attempts + 1):

        req = Request(
            url,
            data=data,
            headers=headers,
            method=method,
        )

        try:

            with urlopen(
                req,
                timeout=TIMEOUT
            ) as response:

                return response.read()

        except HTTPError:

            # Authentication and server errors
            # must not be retried blindly.

            raise

        except (
            URLError,
            OSError,
            HTTPException,
        ) as error:

            if attempt >= attempts:
                raise

            print(
                f"[RETRY] Read interrupted "
                f"({attempt}/{attempts})"
            )

            print(f"        {error}")

            time.sleep(
                RETRY_DELAY * attempt
            )


# ======================================================
# Authentication
# ======================================================

def make_authorization(password):

    credentials = f"{USERNAME}:{password}"

    encoded = base64.b64encode(
        credentials.encode("utf-8")
    ).decode("ascii")

    return f"Basic {encoded}"


# ======================================================
# Check remote file
# ======================================================

def remote_file_matches(host, name, content):

    remote_path = (
        "/" if name == "index.html"
        else "/" + name
    )

    try:

        remote_content = request(
            host + remote_path
        )

        return remote_content == content

    except HTTPError as error:

        if error.code == 404:
            return False

        raise


# ======================================================
# Upload file
# ======================================================

def upload_file(
    host,
    authorization,
    name,
    content,
):

    query = urlencode({
        "name": name
    })

    url = f"{host}/api/dev/web?{query}"

    try:

        result = request(
            url,
            authorization=authorization,
            method="PUT",
            data=content,
            content_type="text/plain",
        )

        result = json.loads(
            result.decode("utf-8")
        )

    except HTTPError as error:

        details = error.read().decode(
            "utf-8",
            errors="replace"
        )

        raise RuntimeError(
            f"Upload rejected for {name}: "
            f"HTTP {error.code} {details}"
        ) from error

    except (
        URLError,
        OSError,
        HTTPException,
        ValueError,
    ) as error:

        raise RuntimeError(
            f"Upload not confirmed for {name}: "
            f"{error}. "
            "The file may have been written. "
            "Run the updater again without --force."
        ) from error

    if not isinstance(result, dict):

        raise RuntimeError(
            f"Invalid upload response: {name}"
        )

    if not result.get("ok"):

        raise RuntimeError(
            f"Upload failed: {name}"
        )

    if result.get("size") != len(content):

        raise RuntimeError(
            f"File size mismatch: {name}"
        )

    # Allow the ESP8266 to finish processing
    # before requesting the uploaded file.

    time.sleep(VERIFY_DELAY)

    try:

        verified = remote_file_matches(
            host,
            name,
            content,
        )

    except (
        URLError,
        OSError,
        HTTPException,
    ) as error:

        raise RuntimeError(
            f"Upload confirmed but verification "
            f"failed for {name}: {error}. "
            "Run the updater again without --force."
        ) from error

    if not verified:

        raise RuntimeError(
            f"Upload confirmed but remote content "
            f"does not match: {name}"
        )


# ======================================================
# Main
# ======================================================

def main():

    parser = argparse.ArgumentParser(
        description="Wordclock web updater"
    )

    parser.add_argument(
        "--host",
        default=DEFAULT_HOST,
        help="Wordclock URL or IP address",
    )

    parser.add_argument(
        "--force",
        action="store_true",
        help="Upload all web files",
    )

    args = parser.parse_args()

    host = args.host.rstrip("/")

    if not host.startswith(
        ("http://", "https://")
    ):

        host = "http://" + host

    print()
    print("Wordclock Web Updater")
    print("=====================")
    print(f"Destination: {host}")
    print(f"Source: {WEB_DIR}")
    print()

    password = getpass.getpass(
        "Web update password: "
    )

    authorization = make_authorization(
        password
    )

    try:

        # Check update availability.

        result = request(
            host + "/api/dev/web",
            authorization=authorization,
        )

        status = json.loads(
            result.decode("utf-8")
        )

        if not status.get("enabled"):

            raise RuntimeError(
                "Web update is disabled"
            )

        max_size = int(
            status["maxFileSize"]
        )

        # Validate every local file
        # before uploading.

        local_files = {}

        for name in FILES:

            path = WEB_DIR / name

            if not path.is_file():

                raise RuntimeError(
                    f"Missing file: {path}"
                )

            content = path.read_bytes()

            if not content:

                raise RuntimeError(
                    f"Empty file: {name}"
                )

            if len(content) > max_size:

                raise RuntimeError(
                    f"{name} exceeds "
                    f"maximum size "
                    f"({max_size} bytes)"
                )

            local_files[name] = content

        print()
        print(f"Connected to {host}")
        print()

        uploaded = 0
        skipped = 0

        for name in FILES:

            content = local_files[name]

            if not args.force:

                if remote_file_matches(
                    host,
                    name,
                    content,
                ):

                    print(
                        f"[SKIP] {name} - unchanged"
                    )

                    skipped += 1

                    continue

            print(
                f"[UPLOAD] {name} "
                f"({len(content)} bytes)"
            )

            upload_file(
                host,
                authorization,
                name,
                content,
            )

            print(
                f"[OK] {name} - verified"
            )

            uploaded += 1

        print()
        print("=====================")
        print("Web update successful")
        print(f"Uploaded: {uploaded}")
        print(f"Unchanged: {skipped}")
        print()
        print("Settings and themes preserved.")
        print(f"Open: {host}")
        print()

        return 0

    except HTTPError as error:

        print()
        print(f"HTTP error: {error.code}")

        if error.code == 401:

            print(
                "Authentication failed. "
                "Check your password."
            )

        else:

            print(
                error.read().decode(
                    "utf-8",
                    errors="replace"
                )
            )

    except (
        URLError,
        OSError,
        HTTPException,
        ValueError,
        RuntimeError,
    ) as error:

        print()
        print(f"Error: {error}")

    except KeyboardInterrupt:

        print()
        print("Update cancelled.")

    print()
    print("Web update failed.")

    return 1


if __name__ == "__main__":

    sys.exit(main())
