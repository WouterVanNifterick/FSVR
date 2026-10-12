# Writes plugin/generated/contributors.txt: the repository's GitHub contributors other than its two collaborators
# (jameshansen and rgwan, credited by name in the About box above them) and bots, each by the name on their GitHub
# profile, else their handle, most contributions first, on one line. The plug-in embeds the file and the About box
# lists it. The build workflow runs this before every build, so a release credits whoever has contributed by then.
# GitHub's contributors list is a cached statistic that can lag a merge by hours (v0.5.2 shipped without
# WouterVanNifterick, merged that day), so the commits themselves are counted too: each one's GitHub author and
# every Co-authored-by line with a GitHub noreply address.
#   python tools/contributors.py [owner/repo]      (GITHUB_TOKEN, when set, lifts the API's rate limit)
# When GitHub cannot be reached the file is left as it is and the build goes on with it.
import json, os, re, sys, urllib.request
from collections import Counter
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / 'plugin' / 'generated' / 'contributors.txt'
COLLABORATORS = {'jameshansen', 'rgwan'}


def get(url):
    req = urllib.request.Request(url, headers={'Accept': 'application/vnd.github+json', 'User-Agent': 'fsvr-contributors'})
    if os.environ.get('GITHUB_TOKEN'):
        req.add_header('Authorization', 'Bearer ' + os.environ['GITHUB_TOKEN'])
    with urllib.request.urlopen(req, timeout=20) as r:
        return json.load(r)


NOREPLY = re.compile(r'^co-authored-by:.*<(?:\d+\+)?([A-Za-z0-9-]+)@users\.noreply\.github\.com>', re.I | re.M)


def commit_logins(commits):
    """Each commit's GitHub author and its noreply co-authors, counted."""
    n = Counter()
    for c in commits:
        if (c.get('author') or {}).get('login'):
            n[c['author']['login']] += 1
        n.update(NOREPLY.findall(c['commit']['message']))
    return n


def ranked(people, commits):
    """The contributors list joined with the commits' authors, most contributions first."""
    n = Counter()
    kind = {}
    for p in people:
        n[p['login']] = max(n[p['login']], p.get('contributions', 0))
        kind[p['login']] = p.get('type')
    for login, k in commit_logins(commits).items():
        n[login] = max(n[login], k)
    return [{'login': l, 'type': kind.get(l)} for l, _ in sorted(n.items(), key=lambda e: -e[1])]


def pages(url):
    out, page = [], 1
    while True:
        batch = get('%s%sper_page=100&page=%d' % (url, '&' if '?' in url else '?', page))
        out += batch
        if len(batch) < 100:
            return out
        page += 1


def names(people, profile):
    """The credit line: each contributor's profile name, else their handle, collaborators and bots left out."""
    out = []
    for p in people:
        login = p['login']
        if login.lower() in COLLABORATORS or p.get('type') == 'Bot' or login.endswith('[bot]'):
            continue
        name = (profile(login) or {}).get('name') or login
        if name not in out:
            out.append(name.strip())
    return ', '.join(out)


def main():
    repo = sys.argv[1] if len(sys.argv) > 1 else 'musicastudio/FSVR'
    try:
        people = ranked(pages('https://api.github.com/repos/%s/contributors' % repo),
                        pages('https://api.github.com/repos/%s/commits' % repo))
        line = names(people, lambda login: get('https://api.github.com/users/' + login))
    except Exception as e:   # offline, or rate limited: keep what is there
        print('contributors: GitHub not reached (%s), %s left as it is' % (e, OUT.name))
        return
    OUT.write_text(line + '\n', encoding='utf-8', newline='\n')
    print('contributors:', line or '(none besides the collaborators)')


if __name__ == '__main__':
    if sys.argv[1:] == ['--check']:   # the filter, without the network
        people = [{'login': 'jameshansen'}, {'login': 'rgwan'}, {'login': 'mkruselj'}, {'login': 'dependabot[bot]', 'type': 'Bot'}, {'login': 'someone'}]
        line = names(people, lambda login: {'mkruselj': {'name': 'Mario Kruselj'}}.get(login))
        assert line == 'Mario Kruselj, someone', line
        # a merge the contributors list has not caught up with, and a co-author credited only in a message
        commits = [{'author': {'login': 'newcomer'}, 'commit': {'message': 'x'}},
                   {'author': {'login': 'jameshansen'}, 'commit': {'message': 'y\n\nCo-authored-by: Pat <5777405+pat-q@users.noreply.github.com>\nCo-authored-by: rgwan <h@iloli.bid>'}}]
        line = names(ranked([{'login': 'jameshansen', 'contributions': 9}, {'login': 'someone', 'contributions': 3}], commits), lambda login: None)
        assert line == 'someone, newcomer, pat-q', line
        print('contributors: --check passes')
    else:
        main()
