#ifndef BSPLINETRACKDEFINTION_H
#define BSPLINETRACKDEFINTION_H

#include "FBXNIFPluginNS.h"
#include <nifparse/Symbol.h>

namespace fbxnif {
	struct BSplineTrackDefinition {
		Symbol handleKey;
		Symbol offsetKey;
		Symbol halfRangeKey;
	};
}

#endif

