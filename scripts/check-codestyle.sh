#!/bin/bash
#
# Check the code style of the changed lines of one C/C++ file.
#
#   scripts/check-codestyle.sh <file> [base]
#
# Without a base, the uncommitted changes of the file are checked. With one (a commit or a branch such as
# origin/master), everything that changed since the current branch left it is checked, committed or not; this
# is what CI does with the base of a pull request. Only the changed functions are looked at, so functions that
# were not touched are never reported.
#
# astyle versions format some constructs differently (lambdas, continuation lines), so the version is pinned
# in .astyle-version and the check refuses to run with another one. The script uses, in this order: the binary
# named by ASTYLE_BIN, an "astyle" on the PATH if it is the pinned version, or uvx (from uv), which fetches the
# pinned version by itself. Without uv, install it with: pip install astyle==<version>

FILE_TO_CHECK=${1}
BASE_COMMIT=HEAD
RC=0

if [[ -n "${2}" ]]; then
    # Compare with the point where the current branch left the base, not with the tip of the base.
    BASE_COMMIT=$(git merge-base "${2}" HEAD 2>/dev/null)
    if [[ -z "${BASE_COMMIT}" ]]; then
        echo "check-codestyle.sh: no common ancestor of '${2}' and HEAD"
        exit 2
    fi
fi

# No "(.*?)" here: the macOS regex library rejects it as invalid, and the test then fails for every file.
if [[ ! -f "${FILE_TO_CHECK}" ]] || [[ ! "${FILE_TO_CHECK}" =~ \.(cxx|cpp|c|h)$ ]]; then
    exit ${RC}
fi

ASTYLE_VERSION_FILE="$(dirname "${BASH_SOURCE[0]}")/../.astyle-version"
ASTYLE_VERSION=""
if [[ -f "${ASTYLE_VERSION_FILE}" ]]; then
    ASTYLE_VERSION=$(tr -d '[:space:]' < "${ASTYLE_VERSION_FILE}")
fi
ASTYLE_FOUND=$("${ASTYLE_BIN:-astyle}" --version 2>/dev/null)
if [[ -z "${ASTYLE_VERSION}" ]] || [[ "${ASTYLE_FOUND}" == "Artistic Style Version ${ASTYLE_VERSION}" ]]; then
    ASTYLE=("${ASTYLE_BIN:-astyle}")
elif [[ -z "${ASTYLE_BIN}" ]] && command -v uvx > /dev/null 2>&1; then
    ASTYLE=(uvx --quiet --from "astyle==${ASTYLE_VERSION}" astyle)
else
    echo "check-codestyle.sh: astyle ${ASTYLE_VERSION} is required (see .astyle-version), found:" \
         "${ASTYLE_FOUND:-no astyle}."
    echo "Install uv (the right version is then fetched automatically), run 'pip install astyle==${ASTYLE_VERSION}'," \
         "or point ASTYLE_BIN at a matching binary."
    exit 2
fi

TMP_FILE_DIFF=$(mktemp /tmp/indi.diff.XXXXXXXXX)
TMP_FILE_ASTYLE=$(mktemp /tmp/indi.astyle.XXXXXXXXX)
TMP_FILE_WARNINGS=$(mktemp /tmp/indi.warning.XXXXXXXXX)
ASTYLE_OPTIONS="--style=allman --align-reference=name --indent-switches --indent-modifiers --indent-classes --pad-oper --indent-col1-comments --lineend=linux --max-code-length=124"

# Keep the new side of the diff: drop the file header (everything up to the first hunk), the hunk headers,
# the removed lines and the "no newline" markers, then strip the leading diff column.
git diff --function-context --unified=1 "${BASE_COMMIT}" -- "${FILE_TO_CHECK}" | sed -e '1,/^@@/d;/^@@/d;/^-/d;/^\\/d;s/^+/ /' | cut -d' ' -f2- > ${TMP_FILE_DIFF}
if ! "${ASTYLE[@]}" ${ASTYLE_OPTIONS} -n < ${TMP_FILE_DIFF} > ${TMP_FILE_ASTYLE}; then
    echo "check-codestyle.sh: could not run astyle ${ASTYLE_VERSION} (${ASTYLE[*]})"
    rm -f ${TMP_FILE_DIFF} ${TMP_FILE_ASTYLE} ${TMP_FILE_WARNINGS}
    exit 2
fi
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
