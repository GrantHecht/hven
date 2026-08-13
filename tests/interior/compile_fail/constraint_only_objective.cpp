// MUST NOT COMPILE.
//
// Registration is per type, not per interface, so a family registered for
// direct storage is reachable from BOTH interfaces. A constraint-only family
// arriving at ObjectiveInterface must therefore say so itself rather than
// failing somewhere inside ObjectiveModel<T>'s instantiation. The expected
// diagnostic is the authored "hven objective adapter:" static_assert in
// DirectFunctionModel::install_objective.

#include "adapter_fixture_functions.h"

hven::solvers::ObjectiveInterface probe_constraint_only_objective() {
    const adapter_fixture::StubConstraintOnly f{};
    return hven::solvers::ObjectiveInterface(f);
}
