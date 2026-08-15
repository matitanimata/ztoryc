#!/usr/bin/env python3
"""Estrae il testo ESATTO del parser da ztorymodel.cpp in parser_body.inc.

Estrarre invece di riscrivere e' il punto di questo test: cosi' verifica il
codice che gira davvero, non una copia che puo' divergere.
"""
import os
here = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(here, '..', 'ztorymodel.cpp'), encoding='utf-8').read()
i = src.index('static QString stripSpeakerExtension')
j = src.index('QStringList ZtoryModel::unknownSpeakers')
body = src[i:j]
body = body.replace(
    'QVector<DialogueLine> ZtoryModel::parseDialogue(const QString &text) const {',
    'QVector<DialogueLine> parseDialogue(const QString &text) {')
body = body.replace(
    'for (const Asset &a : m_assets)\n'
    '    if (a.type.compare("Character", Qt::CaseInsensitive) == 0)\n'
    '      uuidByName.insert(a.name.trimmed().toLower(), a.uuid);',
    'for (const QString &n : gCharacters) uuidByName.insert(n.toLower(), "uuid-" + n);')
# speakerAt() non serve al test: si ferma prima
k = body.find('void ZtoryModel::setSpeakerAlias')
if k > 0: body = body[:k]
open(os.path.join(here, 'parser_body.inc'), 'w', encoding='utf-8').write(body)
print('parser_body.inc rigenerato')
