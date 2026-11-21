#!/usr/bin/env python3
"""M8 AP-5 G-S (../DECLARATION.md): applies ONE named mutation to a scratch copy of the code head.
usage: mutate.py <copy-root> <id>      Each mutation is an exact text replacement that must match exactly `count` times (default 1)."""
import sys

SS = 'src/drivers/solve_status.cpp'
RM = 'src/drivers/result_message.cpp'
IP = 'src/drivers/ipm_solver.cpp'
M = {
    # g1: the relabel is missed (the restoration door resolves kStalled again).
    'g1': (SS, '    case IpmPhaseStop::kRestorationLocallyInfeasible:\n        // The situation the SQP engine labels kInfeasible (M8 AP-5 G-S); uncertified:\n        // `infeasibility_certified` stays false.\n        return SolveStatus::kInfeasible;\n',
           '    case IpmPhaseStop::kRestorationLocallyInfeasible:\n        return SolveStatus::kStalled;\n'),
    # g2: the relabelled exit claims a certificate.
    'g2': (IP, '        this->result_.status =\n            resolve_ipm_phase_status(this->result_.status, this->last_stop_reason_);\n',
           '        this->result_.status =\n            resolve_ipm_phase_status(this->result_.status, this->last_stop_reason_);\n        if (this->result_.status == SolveStatus::kInfeasible) {\n            this->result_.infeasibility_certified = true;\n        }\n'),
    # g3: the relabel is applied to the stage-stalled door too.
    'g3': (SS, '    case IpmPhaseStop::kStageStalled:\n        return SolveStatus::kStalled;\n', '    case IpmPhaseStop::kStageStalled:\n        return SolveStatus::kInfeasible;\n'),
    # g4: the message keeps the generic text.
    'g4': (RM, '        if (reason == StopReason::kRestorationLocallyInfeasible) {\n            return "locally infeasible (uncertified: restoration reached a locally infeasible "\n                   "point)";\n        }\n        return "locally infeasible";\n',
           '        return "locally infeasible";\n'),
    # g5: the IPM substitution reads the RAW verdict, so the relabelled exit substitutes again.
    'g5': (IP, '            opts_.common.return_best,\n            detail::resolve_ipm_phase_status(raw, this->last_stop_reason_));',
           '            opts_.common.return_best, raw);'),
}

root, mid = sys.argv[1], sys.argv[2]
ent = M[mid]
path, old, new = ent[0], ent[1], ent[2]
want = ent[3] if len(ent) > 3 else 1
p = root + '/' + path
s = open(p).read()
n = s.count(old)
if n != want:
    print('MATCH_COUNT %s %d (want %d)' % (mid, n, want))
    sys.exit(2)
open(p, 'w').write(s.replace(old, new))
print('APPLIED %s %s' % (mid, path))
