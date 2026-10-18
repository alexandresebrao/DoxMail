#!/usr/bin/python3
# po-rebrand.py PODIR OUTDIR: copies the PODIR/*.po files into OUTDIR with
# "Evolution" as "DoxMail" in the translations, and writes OUTDIR/en.po
# for the English source strings, so the app names itself DoxMail in any
# language while po/ stays Evolution's (upstream merges keep working).
# Untranslated strings that name Evolution get the renamed English text.
# Kept: Evolution-Data-Server / Evolution Data Server, and the messages
# listed in KEEP, which are about Evolution itself.

import glob
import os
import re
import sys

KEEP = (
	"The Evolution Team",
	"cannot start while Evolution is running",
	"upgrade to Evolution 2",
	"Evolution 2",
)

NAME = re.compile(r"\bEvolution\b(?![- ]Data[- ]Server)")


def rename(text):
	return NAME.sub("DoxMail", text)


def entries(path):
	"""Yields lists of lines, one list per entry (blank-line separated)."""
	block = []
	with open(path, encoding="utf-8") as f:
		for line in f:
			if line.strip() == "":
				if block:
					yield block
					block = []
			else:
				block.append(line)
	if block:
		yield block


def split(block):
	"""Returns (head, msgid lines, rest): msgctxt/msgid/msgid_plural lines
	go to the msgid part, msgstr lines to the rest."""
	head, ids, strs = [], [], []
	part = head
	for line in block:
		if line.startswith("#"):
			head.append(line)
			continue
		if line.startswith(("msgctxt", "msgid")):
			part = ids
		elif line.startswith("msgstr"):
			part = strs
		part.append(line)
	return head, ids, strs


def msgid_text(ids):
	out, cur = [], None
	for line in ids:
		if line.startswith("msgctxt"):
			cur = None
			continue
		if line.startswith("msgid"):
			cur = out
			line = line.split(" ", 1)[1]
		if cur is not None:
			cur.append(eval(line))
	return "".join(out)


def rebrand_entry(block):
	head, ids, strs = split(block)
	if not ids or any(line.startswith("#~") for line in head):
		return block, None

	text = msgid_text(ids)
	if "Evolution" not in text and not any("Evolution" in line for line in strs):
		return block, None
	if any(keep in text for keep in KEEP):
		return block, None

	# fuzzy translations are not used: the English text shows, so rename that
	fuzzy = any(line.startswith("#,") and "fuzzy" in line for line in head)
	if fuzzy:
		head = [line for line in head if not (line.startswith("#,") and "fuzzy" in line)]
	untranslated = fuzzy or all(line.rstrip("\n").endswith('""') or line.startswith('""') for line in strs if line.strip())
	if text and untranslated:
		# take the English text, renamed
		new = []
		msgid = [l for l in ids if not l.startswith("msgctxt")]
		plural = any(l.startswith("msgid_plural") for l in msgid)
		singular, plural_lines, cur = [], [], None
		for line in msgid:
			if line.startswith("msgid_plural"):
				cur = plural_lines
				line = line.split(" ", 1)[1]
			elif line.startswith("msgid"):
				cur = singular
				line = line.split(" ", 1)[1]
			cur.append(line)
		if plural:
			nforms = len([l for l in strs if l.startswith("msgstr[")])
			for n in range(nforms):
				src = singular if n == 0 else plural_lines
				new.append("msgstr[%d] " % n + rename(src[0]))
				new.extend(rename(l) for l in src[1:])
		else:
			new.append("msgstr " + rename(singular[0]))
			new.extend(rename(l) for l in singular[1:])
		strs = new
	else:
		strs = [rename(line) for line in strs]

	return head + ids + strs, (ids, text)


def main():
	podir, outdir = sys.argv[1], sys.argv[2]
	os.makedirs(outdir, exist_ok=True)

	english = {}
	for path in sorted(glob.glob(os.path.join(podir, "*.po"))):
		out, obsolete, aliases, keys = [], [], [], set()
		first = True
		for block in entries(path):
			if first:
				# the header entry stays as it is
				out.append("".join(block))
				first = False
				continue
			if all(line.startswith("#") for line in block):
				obsolete.append("".join(block))
				continue
			block, found = rebrand_entry(block)
			out.append("".join(block))
			head, ids, strs = split(block)
			ctxt = [l for l in ids if l.startswith("msgctxt")]
			keys.add((ctxt[0] if ctxt else "", msgid_text(ids)))
			if found:
				ids, text = found
				key = (ctxt[0] if ctxt else "", text)
				english.setdefault(key, ids)
				# the same translation for the renamed English text, for
				# sources that already say DoxMail (the metainfo)
				aliases.append((ctxt[0] if ctxt else "", rename(text), [rename(l) for l in ids], strs))
		for ctxt, text, ids, strs in aliases:
			if (ctxt, text) not in keys:
				keys.add((ctxt, text))
				out.append("".join(ids + strs))
		out.extend(obsolete)
		dest = os.path.join(outdir, os.path.basename(path))
		data = "\n".join(out) + "\n"
		if not os.path.exists(dest) or open(dest, encoding="utf-8").read() != data:
			with open(dest, "w", encoding="utf-8") as f:
				f.write(data)

	# en: the English strings that name Evolution, renamed
	out = ['msgid ""\nmsgstr ""\n"Content-Type: text/plain; charset=UTF-8\\n"\n"Plural-Forms: nplurals=2; plural=(n != 1);\\n"\n']
	for ids in english.values():
		block, _ = rebrand_entry(["#\n"] + ids + (['msgstr[0] ""\n', 'msgstr[1] ""\n'] if any(l.startswith("msgid_plural") for l in ids) else ['msgstr ""\n']))
		out.append("".join(block[1:]))
	dest = os.path.join(outdir, "en.po")
	data = "\n".join(out) + "\n"
	if not os.path.exists(dest) or open(dest, encoding="utf-8").read() != data:
		with open(dest, "w", encoding="utf-8") as f:
			f.write(data)


if __name__ == "__main__":
	main()
