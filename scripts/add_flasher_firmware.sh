#!/usr/bin/env bash
# Unpack the published firmware into the site source for both web flashers.
#
# Usage: scripts/add_flasher_firmware.sh <site source>
#
# Needs GH_TOKEN and GITHUB_REPOSITORY. Writes the tags of superseded
# development builds to _stale_development_builds.txt; the caller decides
# whether to delete them.

set -euo pipefail

docs_source="$1"

gh release list \
  --exclude-drafts \
  --exclude-pre-releases \
  --limit 1 \
  --json tagName,isDraft,isPrerelease,publishedAt \
  > _official_releases.json

gh release list \
  --exclude-drafts \
  --limit 1000 \
  --json tagName,name,isDraft,isPrerelease,publishedAt \
  > _releases.json

gh api "repos/$GITHUB_REPOSITORY/branches" \
  --paginate \
  --jq '.[].name' \
  > _branches.txt

official_tag="$(python scripts/select_webflasher_release.py \
  _official_releases.json --channel release --optional)"
python scripts/development_builds.py select \
  _releases.json _branches.txt \
  --stale _stale_development_builds.txt \
  > _development_builds.tsv

if [ -n "$official_tag" ]; then
  echo "Official firmware: $official_tag"
  release_bundle="web-flasher-${official_tag}.zip"
  mkdir -p _release/official "$docs_source/flash"
  gh release download "$official_tag" \
    --pattern "$release_bundle" \
    --dir _release/official
  unzip -q "_release/official/$release_bundle" -d "$docs_source/flash"
else
  echo "No published official firmware was found; skipping it."
fi

# Each branch's newest build goes into its own folder; builds.json lists them.
mkdir -p _release/development "$docs_source/flash/development"
while IFS=$'\t' read -r tag folder; do
  echo "Development firmware: $tag in $folder"
  development_bundle="web-flasher-${tag}.zip"
  gh release download "$tag" \
    --pattern "$development_bundle" \
    --dir _release/development
  unzip -q "_release/development/$development_bundle" \
    -d "$docs_source/flash/development/$folder"
done < _development_builds.tsv

python scripts/development_builds.py index "$docs_source/flash/development"
