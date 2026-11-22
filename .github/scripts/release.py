#!/usr/bin/env python3
"""Read the version of tessera-plugin.yaml and its lines in CHANGELOG.md, for .github/workflows/build.yml.

Writes .release-notes.md and, in a GitHub Action, `version` and `exists` to GITHUB_OUTPUT: a version whose tag
already exists is not released again.
"""
import os
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]


def main():
    manifest = (ROOT / "tessera-plugin.yaml").read_text(encoding="utf-8")
    found = re.search(r'(?m)^version:\s*["\']?(\d+\.\d+\.\d+)', manifest)
    if not found:
        sys.exit("release: tessera-plugin.yaml has no version")
    version = found[1]

    changelog = (ROOT / "CHANGELOG.md").read_text(encoding="utf-8")
    heading = re.search(
        rf'(?m)^##\s+{re.escape(version)}(?:\s+-\s+\d{{4}}-\d{{2}}-\d{{2}})?\s*$', changelog)
    lines = []
    if heading:
        rest = changelog[heading.end():]
        end = re.search(r'(?m)^##\s', rest)
        lines = (rest[:end.start()] if end else rest).strip().splitlines()
    if not lines:
        lines = [f"Better power display {version}."]

    (ROOT / ".release-notes.md").write_text("\n".join(line.rstrip() for line in lines).strip() + "\n",
                                            encoding="utf-8")

    tags = subprocess.run(["git", "tag", "-l", f"v{version}"], capture_output=True, text=True, check=True)
    exists = bool(tags.stdout.strip())

    output = os.environ.get("GITHUB_OUTPUT")
    if output:
        with open(output, "a", encoding="utf-8") as handle:
            handle.write(f"version={version}\n")
            handle.write(f"exists={'true' if exists else 'false'}\n")
    print(f"release: v{version} ({'already tagged' if exists else 'new'})")


if __name__ == "__main__":
    main()
