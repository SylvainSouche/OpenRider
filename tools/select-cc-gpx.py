#!/usr/bin/env python3
"""Turns the candidates find-cc-gpx.py found into a test manifest: fetches
them (tools/fetch-cc-gpx.sh), keeps one copy of each distinct file (by
SHA-256), keeps GPX tracks of at least MIN_POINTS points, the timed ones
first, and writes the manifest tools/fetch-cc-gpx.sh reads, gzipped.

    select-cc-gpx.py CANDIDATES.tsv WORKDIR OUT.tsv.gz [LIMIT]

Candidates are taken in random order, at most PER_SITE per site, fetched in
batches; with LIMIT, it stops once LIMIT distinct tracks are kept. WORKDIR
receives the fetched files. A maintainer's tool: tests never need
it. Standard library only.
"""
import collections
import gzip
import hashlib
import os
import random
import re
import subprocess
import sys

MIN_POINTS = 50
MAX_BYTES = 5 * 1024 * 1024
PER_SITE = 300  # candidates fetched per site at most: some serve one file under thousands of URLs


BATCH = 400  # candidates fetched between two counts


def assess(path):
    """(digest, points, timed, size) of a fetched file, or None."""
    if not os.path.exists(path) or os.path.getsize(path) > MAX_BYTES:
        return None
    data = open(path, "rb").read()
    text = data.decode("utf-8", "replace")
    points = len(re.findall(r"<trkpt[\s>]", text))
    times = len(re.findall(r"<time>", text))
    return hashlib.sha256(data).hexdigest(), points, 1 if points and times >= points * 0.9 else 0, len(data)


def main():
    candidates, workdir, out = sys.argv[1:4]
    limit = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    rows = [line.rstrip("\n").split("\t") for line in open(candidates)]
    # A random sample of each site, at most PER_SITE: fewer wasted
    # downloads of the same file, and a varied corpus.
    random.seed(20261024)
    random.shuffle(rows)
    per_site = collections.Counter()
    sampled = []
    for row in rows:
        site = row[0].split("/")[2] if row[0].count("/") >= 2 else row[0]
        per_site[site] += 1
        if per_site[site] <= PER_SITE:
            sampled.append(row)
    os.makedirs(workdir, exist_ok=True)
    tool = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fetch-cc-gpx.sh")
    kept, seen, tried = [], set(), 0
    # In batches, until `limit` distinct tracks are kept (or all are tried).
    for first in range(0, len(sampled), BATCH):
        batch = sampled[first:first + BATCH]
        fetch_list = os.path.join(workdir, "batch.tsv")
        with open(fetch_list, "w") as listing:
            for index, (url, warc, offset, length, crawl) in enumerate(batch, first):
                listing.write(f"c{index}\t{warc}\t{offset}\t{length}\t-\t{url}\n")
        subprocess.run(["sh", tool, fetch_list, workdir], check=False)
        for index, (url, warc, offset, length, crawl) in enumerate(batch, first):
            tried += 1
            found = assess(os.path.join(workdir, f"c{index}.gpx"))
            if not found or found[0] in seen:
                continue
            seen.add(found[0])
            digest, points, timed, size = found
            if points >= MIN_POINTS:
                kept.append((timed, points, digest, url, warc, offset, length, crawl, size))
        print(f"select-cc-gpx: {tried} tried, {len(kept)} kept ({sum(k[0] for k in kept)} timed)", flush=True)
        if limit and len(kept) >= limit:
            break
    kept.sort(key=lambda k: (-k[0], k[3]))
    if limit:
        kept = kept[:limit]
    with gzip.open(out, "wt") as manifest:
        manifest.write("# GPX tracks for the scaling test (scale.tst), fetched from Common Crawl's\n"
                       "# archives by tools/fetch-cc-gpx.sh (bmake fetch-scale-data); never stored\n"
                       "# in this repository: they belong to the sites they were published on.\n"
                       "# Found by tools/find-cc-gpx.py, selected by tools/select-cc-gpx.py: distinct\n"
                       f"# files, tracks of {MIN_POINTS} points or more, timed first.\n"
                       "# Columns: name, warc_file, offset, length, sha256, url, crawl, bytes,\n"
                       "# points, timed.\n")
        for timed, points, digest, url, warc, offset, length, crawl, size in kept:
            manifest.write(f"s{digest[:16]}\t{warc}\t{offset}\t{length}\t{digest}\t{url}\t{crawl}\t{size}\t{points}\t{timed}\n")
    print(f"select-cc-gpx: {len(rows)} candidates, {len(sampled)} sampled, {tried} tried, {len(seen)} distinct files, "
          f"{len(kept)} kept ({sum(k[0] for k in kept)} timed, {sum(k[8] for k in kept) / 1e6:.0f} MB)")


if __name__ == "__main__":
    main()
