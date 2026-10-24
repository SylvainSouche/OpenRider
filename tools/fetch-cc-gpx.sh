#!/bin/sh
# @impl 0f87-6adc-466b-8604
# Fetches the GPX tracks a manifest lists from Common Crawl's archives into a
# local directory, without anything third-party being stored in the
# repository: each line of the manifest gives where Common Crawl archived the
# file (WARC file, byte offset, length), and the SHA-256 of the GPX expected.
#
#   tools/fetch-cc-gpx.sh MANIFEST OUTDIR
#
# MANIFEST may be gzipped (.gz).
# Manifest: tab-separated, '#' comments; columns:
#   name  warc_file  offset  length  sha256  url  (more columns are ignored)
# A track already in OUTDIR with the right SHA-256 is not fetched again. A
# sha256 of "-" accepts whatever is fetched (to build a manifest). Columns
# are never empty: the shell's read merges consecutive tabs.
# Needs curl, gzip, awk and a SHA-256 tool (sha256sum, shasum, openssl or
# FreeBSD's sha256).
set -u
manifest=$1
outdir=$2
base=${CC_BASE_URL:-https://data.commoncrawl.org}
# Politeness: a pause between downloads, and a growing wait when Common
# Crawl refuses (403 or 503 when asked too much).
pause=${CC_PAUSE_S:-0.3}
mkdir -p "$outdir" || exit 1

sha256() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
    elif command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | cut -d' ' -f1
    elif command -v openssl >/dev/null 2>&1; then openssl dgst -sha256 -r "$1" | cut -d' ' -f1
    else sha256 -q "$1"; fi
}

# The manifest's lines, decompressed if it is gzipped.
manifest_lines() {
    case $manifest in
    *.gz) gzip -dc "$manifest" ;;
    *) cat "$manifest" ;;
    esac
}

# The HTTP body of the WARC record on stdin: after the WARC headers and the
# HTTP headers, exactly the WARC Content-Length minus the HTTP headers.
body() {
    LC_ALL=C awk '
        BEGIN { state = 0; used = 0 }
        state == 0 {
            if (tolower($0) ~ /^content-length:/) { sub(/\r$/, ""); split($0, f, ":"); length_left = f[2] + 0 }
            if ($0 == "\r") state = 1
            next
        }
        {
            line = $0 "\n"
            n = length(line)
            if (used + n > length_left) { line = substr(line, 1, length_left - used); n = length(line) }
            used += n
            if (state == 1) { if ($0 == "\r") state = 2; if (used >= length_left) exit; next }
            printf "%s", line
            if (used >= length_left) exit
        }'
}

fetched=0 kept=0 failed=0
while IFS='	' read -r name warc offset length expected url rest; do
    case $name in ''|'#'*) continue ;; esac
    file=$outdir/$name.gpx
    [ "$expected" = "-" ] && expected=
    if [ -f "$file" ] && [ -n "$expected" ] && [ "$(sha256 "$file")" = "$expected" ]; then
        kept=$((kept + 1))
        continue
    fi
    last=$((offset + length - 1))
    ok=no
    for wait in 30 60 120 240 0; do
        sleep "$pause"
        status=$(curl -sS -o "$file.gz" -w '%{http_code}' -r "$offset-$last" "$base/$warc" 2>/dev/null)
        case $status in
        206|200) ok=yes; break ;;
        403|429|503) [ "$wait" -eq 0 ] && break
            echo "fetch-cc-gpx: refused ($status), waiting $wait s" >&2
            sleep "$wait" ;;
        *) break ;;
        esac
    done
    if [ $ok != yes ] || ! gzip -dc < "$file.gz" 2>/dev/null | body > "$file.tmp"; then
        rm -f "$file.gz"
        echo "fetch-cc-gpx: $name: cannot fetch $warc at $offset" >&2
        rm -f "$file.tmp"
        failed=$((failed + 1))
        continue
    fi
    rm -f "$file.gz"
    actual=$(sha256 "$file.tmp")
    if [ -n "$expected" ] && [ "$actual" != "$expected" ]; then
        echo "fetch-cc-gpx: $name: SHA-256 $actual, expected $expected" >&2
        rm -f "$file.tmp"
        failed=$((failed + 1))
        continue
    fi
    mv "$file.tmp" "$file"
    fetched=$((fetched + 1))
done <<MANIFEST
$(manifest_lines)
MANIFEST
echo "fetch-cc-gpx: $fetched fetched, $kept already there, $failed failed (into $outdir)"
[ "$failed" -eq 0 ]
