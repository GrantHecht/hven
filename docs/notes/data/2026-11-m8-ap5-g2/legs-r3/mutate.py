#!/usr/bin/env python3
# M8 AP-5 G2 r3: applies mutation <id> (a..l the declared twelve, m..p the C346 extras) to the scratch source copy.
# usage: mutate.py <src-root> <id>
import sys,subprocess,os
root,mid=sys.argv[1],sys.argv[2]
SQP='src/drivers/sqp_solver.cpp'; IPM='src/drivers/ipm_solver.cpp'
M={
 'a':(IPM,"!callback_stop && this->resto_stash_.valid && this->restoration_ &&","false && this->resto_stash_.valid && this->restoration_ &&"),
 'b':(IPM,"if (newton_form_installed && this->bounds_) {","if (false && newton_form_installed && this->bounds_) {"),
 'c':(IPM,"(this->solve_exit_restoration_active_ && !this->solve_exit_stash_point_);","this->solve_exit_restoration_active_;"),
 'd':(SQP,"if ((feasible_resume || reduction_resume) && !stopped_in_recovery) {","if ((feasible_resume || reduction_resume)) {"),
 'e':(SQP,"return_entry_point();","(void)0;"),
 'f':(SQP,"sub_budget.stop = st.stop_token;","(void)sub_budget;"),
 'g':(SQP,"""                if (mj.soc_applied) {
                    --st.out.counters.soc_applied;
                }""","""                if (false && mj.soc_applied) {
                    --st.out.counters.soc_applied;
                }"""),
 'h':(SQP,"""        st.interrupted = true;
        st.token_interrupt = true;
    }
    const bool stopped_in_recovery""","""        st.interrupted = true;
    }
    const bool stopped_in_recovery"""),
 'i':(SQP,"""                if (mj.failure_step) {
                    --st.out.counters.qp_failure_steps_accepted;
                }""","""                if (false && mj.failure_step) {
                    --st.out.counters.qp_failure_steps_accepted;
                }"""),
 'j':(IPM,".recovering = restoration_contaminated,",".recovering = false,"),
 'k':(IPM,"stash.mu = mu;","stash.mu = 0.0;"),
 'l':(IPM,"stash.row = entry_row;","(void)entry_row;"),
 # extras for the C346 riders
 'm':(SQP,"""        // The stop landed on the converged phase's own terminal row: no-op, so the
        // certified (or no-claim) verdict above is the exit.
        st.interrupted = false;""","""        // MUTANT: latch kept"""),
 'n':(SQP,"const bool stopped_in_recovery = rs.status == SolveStatus::kInterrupted && entry_stash.valid;","const bool stopped_in_recovery = st.interrupted && entry_stash.valid;"),
 'o':(SQP,"st.out.returned_pre_restoration_point = true;","(void)0;"),
 'p':(IPM,"this->result_.returned_pre_restoration_point = stash_exit;","(void)stash_exit;"),
}
f,old,new=M[mid]
p=os.path.join(root,f)
s=open(p).read()
c=s.count(old)
if mid=='e': assert c==2,c
else: assert c==1,(mid,c)
open(p,'w').write(s.replace(old,new))
print("applied",mid,f)
