# Builds the Sticky's "This Day in History" list (3 items a day) from Wikipedia's day pages.
#   python3 art/build_history.py                    download the 366 day pages (once), build ../history_data.h
#   python3 art/build_history.py --topics my.json   pick items about your topics first (see history_topics.example.json)
#   python3 art/build_history.py --empty            build without history (the History page says how to add it)
# The text comes from Wikipedia (CC BY-SA 4.0), so the generated file is not shipped with the project.
# Topics: art/history_topics.json if it exists (git-ignored), else world events only.
import re, sys, os, json, html, time, argparse, urllib.request, urllib.parse, unicodedata

HERE = os.path.dirname(os.path.abspath(__file__))
ap = argparse.ArgumentParser()
ap.add_argument('--topics', default=os.path.join(HERE, 'history_topics.json'))
ap.add_argument('--pages', default=os.path.join(HERE, 'wikidays'), help='folder for the downloaded day pages')
ap.add_argument('--out', default=os.path.join(HERE, '..', 'history_data.h'))
ap.add_argument('--empty', action='store_true')
args = ap.parse_args()

HEAD = ('// "This Day in History" for the Sticky: 3 items a day, built from Wikipedia\'s day pages by art/build_history.py\n'
        '// One line per day: MMDD<tab>year|text<tab>year|text...\n#pragma once\n')

def write_header(tsv):
    with open(args.out, 'w') as f:
        f.write(HEAD + 'static const char STICKY_HISTORY[] = R"HIST(' + tsv + ')HIST";\n')

if args.empty:
    write_header('')
    print('wrote an empty', args.out)
    sys.exit(0)

# ---- topics (weights decide what is picked first) ----
CFG = {'topics': [], 'ignore': '', 'feasts': '', 'days': {}}
if os.path.exists(args.topics):
    CFG.update(json.load(open(args.topics)))
    print('topics from', args.topics)
else:
    print('no topics file: world events only')
KW = [(t['weight'], t['tag'], t['pattern']) for t in CFG['topics']]
# topics whose holidays are only taken when they match "feasts" (e.g. skip minor saints' days)
FEAST_ONLY = {t['tag'] for t in CFG['topics'] if t.get('holidays') == 'feasts_only'}
IGNORE = CFG['ignore']
FEASTS = CFG['feasts']
OVERRIDES = {k: (v[0], v[1], [v[2]]) for k, v in CFG['days'].items()}
WORLD = r'World War|United Nations|\bMoon\b|independence|Independence|\bNASA\b|first (?:woman|man|person|human)|Nobel|Olympic|treaty|Treaty|declaration|Declaration|revolution|Revolution|abolish|landing|founded'
# words that lift a topic item above the others (leaders, founders, firsts)
PROMINENT = CFG.get('prominent', r'Prime Minister|President of|freedom fighter|independence|Independence|saint|Pope|Nobel|founder|first')

# ---- download the day pages once ----
MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December']
LENS = [31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
os.makedirs(args.pages, exist_ok=True)
for m, n in enumerate(LENS):
    for d in range(1, n + 1):
        path = os.path.join(args.pages, f'{m + 1:02d}-{d:02d}.txt')
        if os.path.exists(path): continue
        url = 'https://en.wikipedia.org/w/index.php?' + urllib.parse.urlencode({'title': f'{MONTHS[m]}_{d}', 'action': 'raw'})
        # WHY: Wikipedia asks scripts to name themselves; anonymous fast requests get blocked
        req = urllib.request.Request(url, headers={'User-Agent': 'sticky-history-builder/1.0 (one-off download of 366 pages)'})
        with urllib.request.urlopen(req, timeout=30) as r: open(path, 'wb').write(r.read())
        time.sleep(0.2)
        print('downloaded', os.path.basename(path), end='\r')

def clean(t):
    t = re.sub(r'<ref[^>]*/>', '', t); t = re.sub(r'<ref[^>]*>.*?</ref>', '', t, flags=re.S)
    t = re.sub(r'<!--.*?-->', '', t, flags=re.S)
    for _ in range(3): t = re.sub(r'\{\{(?:nowrap|lang|transl|nbsp|sic|small|ill|interlanguage link)\|(?:[^{}|]*\|)?([^{}|]*)[^{}]*\}\}', r'\1', t)
    t = re.sub(r'\{\{[^{}]*\}\}', '', t); t = re.sub(r'\{\{[^{}]*\}\}', '', t)
    t = re.sub(r'\[\[(?:[^\]|]*\|)?([^\]]*)\]\]', r'\1', t)
    t = re.sub(r"'''?", '', t); t = html.unescape(t)
    t = re.sub(r'<[^>]+>', '', t)
    t = t.replace('&nbsp;', ' ').replace('–', '-').replace('—', '-').replace('’', "'").replace('‘', "'").replace('“', '"').replace('”', '"')
    t = re.sub(r'\s+', ' ', t).strip(' *:-')
    return t

def ascii_ok(t):
    # the Sticky's fonts carry Latin letters only; drop accents, skip the rest
    return unicodedata.normalize('NFKD', t).encode('ascii', 'ignore').decode()

def shorten(t, n=96):
    t = re.sub(r'\s*\((?:d\.|b\.|died|born) [^)]*\)', '', t).strip()
    if len(t) <= n: return t
    cut = t[:n]
    for sep in ['; ', ', ', ' - ', ' ']:
        i = cut.rfind(sep)
        if i > n * 0.6: return cut[:i].rstrip(' ,;-') + '...'
    return cut.rstrip() + '...'

def parse(path):
    txt = open(path, encoding='utf-8', errors='ignore').read()
    secs = re.split(r'\n==\s*([^=]+?)\s*==\s*\n', txt)
    out = {'Events': [], 'Births': [], 'Deaths': [], 'Holidays': []}
    for i in range(1, len(secs) - 1, 2):
        name = secs[i].strip(); body = secs[i + 1]
        key = 'Holidays' if name.startswith('Holidays') else name if name in out else None
        if not key: continue
        year_ctx = None
        for line in body.split('\n'):
            if not line.startswith('*'): continue
            depth = len(line) - len(line.lstrip('*'))
            raw = line.lstrip('*').strip()
            if key == 'Holidays':
                c = clean(raw)
                if c: out[key].append((None, c, depth)); continue
            m = re.match(r'(?:\{\{anchor\|[^}]*\}\})?\s*\[\[(?:AD )?(\d{1,4})(?: BC)?\]\]\s*(?:&ndash;|–|-|&mdash;|—)\s*(.*)', raw)
            if m:
                y, rest = m.group(1), m.group(2)
                if rest.strip(): out[key].append((int(y), clean(rest), depth))
                year_ctx = int(y)
            elif depth >= 2 and year_ctx:
                c = clean(raw)
                if c: out[key].append((year_ctx, c, depth))
    return out

def score(text):
    s, tags = 0, []
    if IGNORE: text = re.sub(IGNORE, '', text)
    for w, tag, pat in KW:
        if re.search(pat, text): s += w; tags.append(tag)
    return s, tags

days = sorted(f for f in os.listdir(args.pages) if f.endswith('.txt'))
lines, stats = [], {'world': 0, 'feast': 0}
for f in days:
    mmdd = f[:5].replace('-', '')
    d = parse(os.path.join(args.pages, f))
    cands = []
    for y, t, _ in d['Events']:
        s, tags = score(t); cands.append((s + 1 + (1 if s and re.search(PROMINENT, t) else 0), y, t, tags, 'event'))
    for y, t, _ in d['Births']:
        s, tags = score(t)
        if s: cands.append((s + (2 if re.search(PROMINENT, t) else 0), y, 'Born: ' + t, tags, 'birth'))
    for y, t, _ in d['Deaths']:
        s, tags = score(t)
        if s >= 5: cands.append((s - 1, y, 'Died: ' + t, tags, 'death'))
    picks = [OVERRIDES[mmdd]] if mmdd in OVERRIDES else []
    # observances first: days matching your topics, and the big feasts
    for _, t, _ in ([] if picks else d['Holidays']):
        s, tags = score(t)
        feast = FEASTS and re.search(FEASTS, t)
        if 'liturgics' in t or 'Christian feast day' in t: continue
        if (s >= 3 and not FEAST_ONLY & set(tags)) or feast:
            picks.append(('Today', shorten(ascii_ok(t)), tags or ['feast']))
            if feast: stats['feast'] += 1
            break
    rel = sorted([c for c in cands if c[3]], key=lambda c: (-c[0], -(c[1] or 0)))
    seen = set()
    for s, y, t, tags, kind in rel:
        if len(picks) >= 3: break
        key = t[:40]
        if key in seen: continue
        seen.add(key); picks.append((str(y), shorten(ascii_ok(t)), tags))
    if len(picks) < 3:
        world = [c for c in cands if not c[3] and c[4] == 'event']
        world.sort(key=lambda c: (0 if re.search(WORLD, c[2]) else 1, -(c[1] or 0)))
        for s, y, t, tags, kind in world:
            if len(picks) >= 3: break
            picks.append((str(y), shorten(ascii_ok(t)), ['world']))
    for p in picks:
        for tg in p[2]: stats[tg] = stats.get(tg, 0) + 1
    lines.append(mmdd + '\t' + '\t'.join(f'{y}|{t}' for y, t, _ in picks[:3]))
write_header('\n'.join(lines) + '\n')
print('days', len(lines), 'bytes', os.path.getsize(args.out), stats)
