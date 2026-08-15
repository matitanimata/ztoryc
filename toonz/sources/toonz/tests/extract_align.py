#!/usr/bin/env python3
"""Estrae il testo ESATTO del riallineamento da ztorymodel.cpp."""
import os
here = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(here, '..', 'ztorymodel.cpp'), encoding='utf-8').read()
i = src.index('static double wordSimilarity')
j = src.index('bool ZtoryModel::speakerAt')
body = src[i:j]
body = body.replace('QVector<TimedWord> ZtoryModel::alignToScript(',
                    'QVector<TimedWord> alignToScript(')
open(os.path.join(here, 'align_body.inc'), 'w', encoding='utf-8').write(body)
print('align_body.inc rigenerato')
