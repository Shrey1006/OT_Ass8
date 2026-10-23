#!/usr/bin/env bash
# ==============================================================================
# log_prompt.sh - Create a timestamped prompt log file for a specific track
# Usage: ./scripts/log_prompt.sh <track_folder_name> [optional_log_name]
# Example: ./scripts/log_prompt.sh A_continuous gd_momentum_test
# ==============================================================================
set -euo pipefail

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <track_name> [title]"
    echo "Available tracks: A_continuous, B_discrete, C_quantum, D_submodular_parallel, E_case_study, F_compiler, G_xv6, H_dbms, I_quantum_algorithms, J_softeng"
    exit 1
fi

TRACK="$1"
TITLE="${2:-prompt_log}"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
HUMAN_DATE=$(date +"%Y-%m-%d %H:%M:%S")

TARGET_DIR="prompts/$TRACK"

if [ ! -d "$TARGET_DIR" ]; then
    mkdir -p "$TARGET_DIR"
fi

FILENAME="${TARGET_DIR}/${TIMESTAMP}_${TITLE}.md"

cat <<LOGEOF > "$FILENAME"
# Prompt Log: ${TRACK} - ${TITLE}
- **Timestamp**: ${HUMAN_DATE}
- **Track**: ${TRACK}
- **AI Tool & Model**: Antigravity (Gemini 3.8 Flash)

## Original Prompt
\`\`\`text
<Enter original prompt here>
\`\`\`

## AI Response (Summary / Relevant Excerpt)
\`\`\`text
<Enter AI response here>
\`\`\`

## Revised Prompt / User Iteration
\`\`\`text
<Enter revised prompt or user follow-up feedback here>
\`\`\`

## Commit Link
- Commit Hash / Tag: \`[AI:Antigravity] <commit_message_or_hash>\`

## Verification Note
1. Correctness Check: <Independently verified outputs and constraints>
2. Baseline & Fair Comparison: <Identical inputs, stopping criteria, and seeds verified>
3. Claims Verified: <No unsupported speedup or theoretical claims made>
LOGEOF

echo "Created prompt log: $FILENAME"
