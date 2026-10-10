#!/usr/bin/env bash
# Update the indiserver Homebrew formula for a released INDI tag.

set -euo pipefail

release_tag="${1:?Usage: $0 <release-tag> [formula-path]}"
formula_path="${2:-Formula/indiserver.rb}"

if [[ ! "$release_tag" =~ ^v[0-9][0-9A-Za-z._-]*$ ]]; then
    echo "Release tag must look like v2.2.4.2; got: $release_tag" >&2
    exit 1
fi
if [[ ! -f "$formula_path" ]]; then
    echo "Formula not found: $formula_path" >&2
    exit 1
fi

archive_url="https://github.com/indilib/indi/archive/refs/tags/${release_tag}.tar.gz"
temporary_dir="$(mktemp -d "${TMPDIR:-/tmp}/indiserver-formula.XXXXXX")"
trap 'rm -rf "$temporary_dir"' EXIT

curl --fail --location --retry 3 --silent --show-error "$archive_url" \
    --output "$temporary_dir/indi.tar.gz"
archive_sha256="$(shasum -a 256 "$temporary_dir/indi.tar.gz" | awk '{print $1}')"

sed -i.bak -E "s|^  url \".*\"$|  url \"$archive_url\"|" "$formula_path"
sed -i.bak -E "s|^  sha256 \"[0-9a-f]{64}\"$|  sha256 \"$archive_sha256\"|" "$formula_path"
rm "$formula_path.bak"

echo "Updated $formula_path for $release_tag"
