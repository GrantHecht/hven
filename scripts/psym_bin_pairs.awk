# P-SYM pair-bin audit. Reads a `psym_compare.sh compare` transcript produced
# with PSYM_MAX_PAIRS=0, no symbol map and no exceptions -- so the UNCLASSIFIED
# pair population it prints is COMPLETE -- and sorts every printed pair into one
# bin. A pair no bin claims is a FINDING and is printed in full.
#
#   usage: awk -f scripts/psym_bin_pairs.awk <transcript>
#
# WHY IT EXISTS. A P-SYM transcript for a layout change (M6 W5 T1's
# member-displacement shift, M6 W5 T6's TU split) is thousands of pairs of one
# or two repeated shapes. "Every pair is the same -8 displacement" is a claim
# that is only checkable against EVERY pair, and a claim checked by eye over
# thousands of lines is not checked. This bins them mechanically; the report
# quotes the bin counts and the OTHER pairs, if any, in full.
#
# THE BINS, and what each asserts:
#
#   D8      a memory displacement that moved by exactly -8 with the rest of the
#           line equal, and no %rsp in it: THE member-offset shift, the eight
#           bytes a dropped vptr frees at the head of every object.
#   FRAME   a stack-slot reference that moved by exactly +/-8, including the
#           `mov %rsp,R` <-> `lea 0x8(%rsp),R` form and the `sub $-0x80` <->
#           `add $0x78` pair (one pointer step, two mnemonics, because the
#           operand crossed the sign boundary): the same eight bytes seen from
#           the frame rather than from the object. A frame-SIZE immediate
#           (`sub $N,%rsp` / `add $N,%rsp`) is also accepted at +/-16, and only
#           there: the ABI keeps %rsp 16-byte aligned, so a caller that holds
#           an object which lost 8 bytes by value re-lays its frame in one
#           16-byte step or none at all. Slots move by 8; the frame moves by 16.
#   VPTR    a vptr load or store present on one side only.
#   V2D     a virtual call turned into a direct one.
#   NOP     both sides are alignment padding of different length.
#   REND    one side renders a control-transfer target absolutely and the other
#           as SELF+off, because objdump's nearest-symbol label crossed the
#           enclosing symbol's range. Since M6 W5 T1 fix2 bounded the SELF rule
#           by the symbol's own SIZE and resolves an out-of-range bare target
#           to the symbol containing it, this bin is empty on every comparison
#           in this tree: it is kept because a symbol whose table entry carries
#           no size still falls back to the label, and a pair from that
#           fallback is this shape.
#   SLIP    exactly one side is alignment padding: the POSITIONAL pairing inside
#           the symbol slipped where a padding run changed length, so every pair
#           after it is an artifact of the pairing and not a comparison.
#   ADDR    a pure ADDRESS move, in one of the three shapes that word actually
#           covers -- see the next block. Reported with its split.
#   RENAME  a relocation record differing only in the callee's demangled name.
#   RDATA   a relocation record whose type and TARGET are identical, whose
#           ADDEND differs, and whose target is POSITIVELY IDENTIFIED AS A
#           NON-EXECUTABLE SECTION. The
#           addend of such a record is a byte offset into a datum this
#           comparison does not read, and it moves whenever anything ahead of
#           that datum in its section changes size -- the same layout property
#           psym_compare.sh's own class (b) and its `+LOCAL` neutralisation
#           already exclude for a SECTION target. It is binned here for the
#           records that class does not reach: a target that NAMES a data
#           symbol keeps its addend verbatim, deliberately, so `_ZN4hven3tblE
#           +0x28` -> `+0x30` arrives as an UNCLASSIFIED pair even though one
#           inserted datum ahead of it is the whole story. Registered by the
#           SQP lane at M6 W5 T5 (M1), whose ten non-caller DIFFERS symbols
#           were exactly this shape and were reported as "real".
#
#           POSITIVE EVIDENCE, NOT THE ABSENCE OF A `.text` PREFIX (M6 W5 T6
#           commit 0 fix2, Codex Important 1). As first landed this bin excluded
#           only names matching `.text`, so a same-target NAMED FUNCTION whose
#           addend moved, and any non-`.text` executable section such as `.init`,
#           were binned RDATA -- which is the opposite of what the paragraph
#           above claims and of what G4 requires. The admission now needs the
#           target to be a section name on an ALLOWLIST of known non-executable
#           sections; a NAMED target (no leading dot) and any section not on the
#           list stay OTHER, where they must be explained rather than counted.
#
#           WHAT THAT COSTS, said plainly rather than left to be discovered: on
#           transcripts THIS tool chain produces the bin is now UNREACHABLE, and
#           deliberately so. psym_compare.sh already neutralises the addend of a
#           non-executable SECTION target to `+LOCAL` (its RELOCATION TARGETS
#           block, the class (b) argument), so two arms can never disagree about
#           one; the only differing-addend pairs that reach a transcript name a
#           SYMBOL, and those are exactly the class this bin may no longer
#           admit without knowing whether the symbol is code or data. The
#           SQP lane's T5 M1 case -- `_ZN4hven3tblE+0x28` -> `+0x30`, one
#           inserted datum ahead of it -- therefore lands in OTHER again and is
#           explained by hand.
#
#           THE ROUTE BACK, registered rather than built here: carry TARGET-KIND
#           metadata from psym_compare.sh into the rendered relocation (it can
#           read the nm type letter of any symbol DEFINED anywhere in the arm),
#           and admit a named target whose kind is data. That is a change to
#           what every transcript looks like and it belongs in its own round.
#
#           THE LIMIT that remains, stated rather than left to be inferred: this
#           bin cannot tell an addend that MOVED from an addend that now names a
#           DIFFERENT datum of the same kind. It is the same limit
#           psym_compare.sh states for its own neutralisations, at the same
#           grain, and it is why the bin is reported with its addend deltas
#           rather than folded into RENAME.
#   OTHER   anything else: a finding.
#
# THE ADDR BIN IS DELIBERATELY NARROW (M6 W5 T1 fix1). It used to be "the two
# sides are equal once every hex literal is blanked", which is not an address
# claim at all: a BASE-REGISTER member displacement that moved by any amount
# other than -8 -- a real layout change, and precisely what a fold or a member
# reorder can get wrong -- satisfied it and vanished into the largest bin in the
# audit. It is now three named shapes, counted separately so the report can show
# the split:
#
#   A_BR    a control-transfer target (`jmp`/`jcc`/`call`/`loop`), bare hex or
#           SELF+off, with the rest of the line equal. The symbol or its target
#           moved within the section.
#   A_RIP   a rip-relative displacement, with the rest of the line equal. The
#           datum's own section moved; the relocation records carry its
#           identity and are compared separately.
#   A_STK   a `(%rsp)`/`(%rbp)` frame slot -- with or without an index register,
#           `0x328(%rsp,%rcx,1)` included -- whose delta is NOT +/-8, with the
#           rest of the line equal: a frame re-layout, which changing a class's
#           size legitimately causes in a caller that holds one by value.
#
# Anything else that used to reach ADDR -- notably a base-register displacement
# that moved by something other than -8 -- now lands in OTHER, where it has to
# be explained rather than counted.

function norm(s) { gsub(/0x[0-9a-f]+/, "H", s); sub(/^(\t[a-z0-9]+ +)[0-9a-f]+$/, "\\1H", s); return s }
function nreloc(s) { gsub(/hven::solvers::(OptimizationProblemBase|NLPSolver)/, "T", s)
                     gsub(/(OptimizationProblemBase|NLPSolver)::JetJobModes/, "T::JetJobModes", s); return s }
# The trailing ` ;PAD=<n>` psym_compare.sh's flatten_symbols() annotates onto a
# pad line (M6 W5 T6 commit 0 fix1) is part of the compared text, so it reaches
# this transcript; accept it here too, or every annotated pad pair falls to
# OTHER. Kept in step with is_pad_line()/is_pad_f() there.
function ispad(s) { sub(/[ \t];PAD=([0-9]+|\?)$/, "", s)
                    return s ~ /^\t((data16 |cs |rex[0-9a-z.]* )*nop[wl]?( +[^ ]+)?|nop|xchg +%ax,%ax)$/ }
# The (%rsp)-relative displacement a line addresses, or "NA". An INDEX register
# is allowed in the memory operand (`0x328(%rsp,%rcx,1)`): it is a runtime
# offset into an array whose BASE is the frame slot, and the displacement is
# still the slot.
function stkval(s,   t, d) {
    if (match(s, /(-?0x[0-9a-f]+)?\(%rsp(,%[a-z0-9]+,[0-9])?\)/)) {
        t = substr(s, RSTART, RLENGTH)
        if (substr(t, 1, 1) == "(") return 0
        sub(/\(.*$/, "", t)
        return strtonum(t) }
    if (s ~ /%rsp,%r/) return 0
    return "NA" }
function stkkey(s) { gsub(/(-?0x[0-9a-f]+)?\(%rsp(,%[a-z0-9]+,[0-9])?\)/, "S", s)
                     sub(/^\tlea +S,/, "\tMOV S,", s)
                     sub(/^\tmov +S,/, "\tMOV S,", s)
                     sub(/^\tmov +%rsp,/, "\tMOV S,", s); return s }
# The signed value an add/sub immediate CONTRIBUTES, so that `sub $-0x80` and
# `add $0x78` -- the same pointer step, 8 bytes apart, rendered by two different
# mnemonics because the operand crossed the sign boundary -- are comparable.
function addend(s,   v, w) {
    if (s !~ /^\t(add|sub)[lqbw]? +\$0x[0-9a-f]+,/) return "NA"
    match(s, /\$0x[0-9a-f]+/); w = substr(s, RSTART + 3, RLENGTH - 3)
    # Sign-extend from the LOW 32 bits: objdump renders a negative 8-bit or
    # 32-bit immediate sign-extended to the operand width, and strtonum on a
    # 16-hex-digit value loses the precision the subtraction would need.
    if (length(w) > 8) w = substr(w, length(w) - 7)
    v = strtonum("0x" w)
    if (v > 2147483647) v = v - 4294967296
    if (s ~ /^\tsub/) v = -v
    return v }
# A frame-SIZE immediate: `sub $N,%rsp` / `add $N,%rsp` and nothing else.
function is_frame_imm(s) { return s ~ /^\t(add|sub)[lqbw]? +\$0x[0-9a-f]+,%rsp$/ }
# A control transfer whose whole operand is the target: bare hex or SELF+off.
function is_branch(s) { return s ~ /^\t(j[a-z]+|call|loop[a-z]*|xbegin) +(SELF\+0x[0-9a-f]+|[0-9a-f]+)$/ }
function brkey(s) { sub(/ +(SELF\+0x[0-9a-f]+|[0-9a-f]+)$/, " T", s); return s }
function ripkey(s) { gsub(/-?0x[0-9a-f]+\(%rip\)/, "RIP", s); return s }
# A relocation record `RELOC <type> <target> <addend>`, split at the LAST
# space: a demangled target contains spaces, an addend never does.
function reloc_head(s) { sub(/ [+-]?(0x[0-9a-f]+|LOCAL)$/, "", s); return s }
function reloc_addend(s) { if (match(s, / [+-]?(0x[0-9a-f]+|LOCAL)$/)) return substr(s, RSTART + 1); return "NA" }
# The target field of a relocation record: everything between the type and the
# addend.
function reloc_target(s,   h) {
    h = reloc_head(s)
    sub(/^RELOC +[^ ]+ +/, "", h)
    return h }
# POSITIVE evidence that a relocation target is NON-EXECUTABLE DATA: it must be
# a section name, and that section must be one this list knows to hold data.
# Anything else -- a named symbol, `.text` and its variants, `.init`/`.fini`,
# a section this list has not seen -- is NOT admitted. See the RDATA block for
# why the test is an allowlist and not "does not look like .text".
function reloc_is_data_section(t) {
    if (substr(t, 1, 1) != ".") return 0
    return t ~ /^\.(rodata|data|bss|sdata|sbss|data\.rel\.ro|tdata|tbss|init_array|fini_array|preinit_array|eh_frame|eh_frame_hdr|gcc_except_table|comment|note)([.$]|$)/ }
function stkonly(s) { gsub(/(-?0x[0-9a-f]+)?\((%rsp|%rbp)(,%[a-z0-9]+,[0-9])?\)/, "S", s); return s }
function has_stk(s) { return s ~ /\((%rsp|%rbp)(,%[a-z0-9]+,[0-9])?\)/ }

/^ *UNCLASSIFIED  - /{ b = $0; sub(/^ *UNCLASSIFIED  - /, "", b); getline a; sub(/^ *\+ */, "", a)
  tot++
  if (b ~ /^RELOC / && a ~ /^RELOC /) {
    if (nreloc(b) == nreloc(a)) { ren++; next }
    if (reloc_head(b) == reloc_head(a) && reloc_is_data_section(reloc_target(b)) &&
        reloc_addend(b) != "NA" && reloc_addend(a) != "NA") {
      rdata++
      rdeltas[reloc_addend(b) " -> " reloc_addend(a)]++
      next } }
  if (ispad(b) && ispad(a)) { nop++; next }
  if (ispad(b) != ispad(a)) { slip++; next }
  if (b ~ /call +\*/ && a ~ /call +(SELF\+)?[0-9a-f]/) { v2d++; next }
  if ((b ~ /call +SELF\+/ && a ~ /call +[0-9a-f]+$/) || (b ~ /call +[0-9a-f]+$/ && a ~ /call +SELF\+/)) { rend++; next }
  db = "NA"; if (match(b, /-?0x[0-9a-f]+\(/)) db = strtonum(substr(b, RSTART, RLENGTH-1))
  da = "NA"; if (match(a, /-?0x[0-9a-f]+\(/)) da = strtonum(substr(a, RSTART, RLENGTH-1))
  if (db != "NA" && da != "NA" && da - db == -8 && norm(b) == norm(a) && b !~ /%rsp/) { d8++; next }
  sb = stkval(b); sa = stkval(a)
  if (sb != "NA" && sa != "NA" && (sa - sb == 8 || sa - sb == -8) && stkkey(b) == stkkey(a)) { frame++; next }
  ib = addend(b); ia = addend(a)
  if (ib != "NA" && ia != "NA" && (ia - ib == 8 || ia - ib == -8)) { frame++; next }
  if (is_frame_imm(b) && is_frame_imm(a) && ib != "NA" && ia != "NA" &&
      (ia - ib == 16 || ia - ib == -16)) { frame++; next }
  if (b ~ /^\t(mov|lea)/ && b ~ /\(%r[a-z0-9]+\)/ && a !~ /\(%r[a-z0-9]+\)/) { vptr++; next }
  # ADDR, in its three documented shapes only.
  if (is_branch(b) && is_branch(a) && brkey(b) == brkey(a)) { a_br++; addr++; next }
  if (b ~ /\(%rip\)/ && a ~ /\(%rip\)/ && ripkey(b) == ripkey(a)) { a_rip++; addr++; next }
  if (has_stk(b) && has_stk(a) && stkonly(b) == stkonly(a)) { a_stk++; addr++; next }
  other++; printf "OTHER  - %s\n       + %s\n", b, a
}
END { printf "BINS  total=%d  D8=%d  FRAME=%d  VPTR=%d  V2D=%d  NOP=%d  REND=%d  SLIP=%d  ADDR=%d (BR=%d RIP=%d STK=%d)  RENAME=%d  RDATA=%d  OTHER=%d\n",
             tot+0, d8+0, frame+0, vptr+0, v2d+0, nop+0, rend+0, slip+0, addr+0, a_br+0, a_rip+0, a_stk+0, ren+0, rdata+0, other+0
      # The RDATA deltas, printed for the same reason class (a) prints its
      # immediate deltas: a shift shared by many records is one inserted datum,
      # a lone odd one is worth reading.
      if (rdata > 0) { printf "RDATA-DELTAS"; for (d in rdeltas) printf "  %s (x%d)", d, rdeltas[d]; printf "\n" } }
