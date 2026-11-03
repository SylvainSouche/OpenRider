#!/usr/bin/env python3
"""Finds the GPX files Common Crawl archived: candidates for OpenRider's
test corpora (see tools/fetch-cc-gpx.sh and testdata/).

    find-cc-gpx.py OUT.tsv CRAWL [CRAWL...]      e.g. CC-MAIN-2026-43

Polite and resumable: two workers (FIND_CC_GPX_WORKERS), a pause between
index parts, and on a refusal (Common Crawl answers 403 or 503 when asked
too much) a growing wait before trying again; the parts done are listed in
OUT.done, and a new run skips them.

Reads each crawl's columnar index over HTTPS with DuckDB (pip install
duckdb) and keeps the records whose MIME type -- declared by the server or
detected from the content -- is GPX. Only the two MIME columns are scanned
(about 0.4 MB per index part, 120 MB per crawl; the URL column alone weighs
some 75 GB per crawl): URLs and archive locations are then read for the
matching rows only. A maintainer's tool, run rarely: tests never need it.

Output (appended, tab-separated): url, warc_file, offset, length, crawl;
a URL already in OUT is not added again.
"""
import concurrent.futures
import gzip
import os
import sys
import threading
import time
import urllib.request

import duckdb

BASE = "https://data.commoncrawl.org/"


def parts(crawl):
    """The crawl's index parts holding WARC records."""
    with urllib.request.urlopen(BASE + "crawl-data/" + crawl + "/cc-index-table.paths.gz") as response:
        paths = gzip.decompress(response.read()).decode().split()
    return [BASE + p for p in paths if "subset=warc" in p]


WORKERS = int(os.environ.get("FIND_CC_GPX_WORKERS", "2"))
PAUSE_S = 1.0          # between two parts, per worker
BACKOFF_S = [30, 60, 120, 240, 480, 960]  # after successive refusals


def search(part):
    """The GPX records of one index part, or None when Common Crawl kept
    refusing (the part is then left for a later run)."""
    con = duckdb.connect()
    con.execute("install httpfs; load httpfs;")
    for wait in BACKOFF_S + [None]:
        try:
            rows = con.execute(
                f"""select url, warc_filename, warc_record_offset, warc_record_length
                    from read_parquet('{part}')
                    where content_mime_detected ilike '%gpx%' or content_mime_type ilike '%gpx%'"""
            ).fetchall()
            time.sleep(PAUSE_S)
            return rows
        except duckdb.Error as error:
            if wait is None:
                print(f"find-cc-gpx: {part.rsplit('/', 1)[-1]}: given up ({str(error).splitlines()[0]})", file=sys.stderr)
                return None
            print(f"find-cc-gpx: refused, waiting {wait} s", file=sys.stderr, flush=True)
            time.sleep(wait)


def main():
    out, crawls = sys.argv[1], sys.argv[2:]
    seen, done = set(), set()
    if os.path.exists(out):
        with open(out) as existing:
            seen = {line.split("\t", 1)[0] for line in existing}
    if os.path.exists(out + ".done"):
        with open(out + ".done") as finished:
            done = set(finished.read().split())
    lock = threading.Lock()
    with open(out, "a") as sink, open(out + ".done", "a") as finished:
        for crawl in crawls:
            todo = [p for p in parts(crawl) if p not in done]
            found = added = given_up = 0

            def run(part):
                nonlocal found, added, given_up
                rows = search(part)
                with lock:
                    if rows is None:
                        given_up += 1
                        return
                    for url, warc, offset, length in rows:
                        found += 1
                        if url not in seen:
                            seen.add(url)
                            added += 1
                            sink.write(f"{url}\t{warc}\t{offset}\t{length}\t{crawl}\n")
                    sink.flush()
                    finished.write(part + "\n")
                    finished.flush()

            with concurrent.futures.ThreadPoolExecutor(max_workers=WORKERS) as pool:
                list(pool.map(run, todo))
            print(f"find-cc-gpx: {crawl}: {len(todo)} parts, {found} GPX records, {added} new URLs, "
                  f"{given_up} parts left for a later run", flush=True)


if __name__ == "__main__":
    main()
