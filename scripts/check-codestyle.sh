#!/bin/bash

FILE_TO_CHECK=${1}
RC=0

if [[ ! -f "${FILE_TO_CHECK}" ]] || [[ ! "${FILE_TO_CHECK}" =~ (.*?)\.(cxx|cpp|c|h)$ ]]; then
    exit ${RC}
fi

TMP_FILE_DIFF=$(mktemp /tmp/indi.diff.XXXXXXXXX)
TMP_FILE_ASTYLE=$(mktemp /tmp/indi.astyle.XXXXXXXXX)
TMP_FILE_WARNINGS=$(mktemp /tmp/indi.warning.XXXXXXXXX)
ASTYLE_OPTIONS="--style=allman --align-reference=name --indent-switches --indent-modifiers --indent-classes --pad-oper --indent-col1-comments --lineend=linux --max-code-length=124"

git diff --function-context --unified=1 HEAD ${FILE_TO_CHECK} | sed -e '/diff --git/,+3d;/^@@/d;/^-/d;s/^+/ /' | cut -d' ' -f2- > ${TMP_FILE_DIFF}
astyle ${ASTYLE_OPTIONS} -n < ${TMP_FILE_DIFF} > ${TMP_FILE_ASTYLE}
diff -u ${TMP_FILE_DIFF} ${TMP_FILE_ASTYLE} | tail -n +3 | sed -e '/^-/d;/^@@/d;s/^+/[STYLE WARNING] /' > ${TMP_FILE_WARNINGS}

if [[ -s ${TMP_FILE_WARNINGS} ]]; then
    cat ${TMP_FILE_WARNINGS}
    RC=1
fi

if [[ ${RC} -eq 1 ]]; then
    echo -e "File ${FILE_TO_CHECK} has style warning(s), see" \
	 "https://github.com/indilib/indi/blob/master/README.md"
fi

rm -f ${TMP_FILE_DIFF} ${TMP_FILE_ASTYLE} ${TMP_FILE_WARNINGS}

exit ${RC}
