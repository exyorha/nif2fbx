#ifndef BSPLINEDATASET_H
#define BSPLINEDATASET_H

#include "FBXNIFPluginNS.h"
#include <nifparse/Types.h>

namespace fbxnif {

	struct BSplineTrackDefinition;

	struct BSplineDataSet {
		enum : int {
			Degree = 3
		};

		BSplineDataSet(const NIFDictionary &interpolator);
		~BSplineDataSet();

		BSplineDataSet(const BSplineDataSet &other) = delete;
		BSplineDataSet &operator =(const BSplineDataSet &other) = delete;

		bool isTrackPresent(const BSplineTrackDefinition &def) const;

		template<size_t PointSize>
		std::vector<std::array<float, PointSize>> extractTrack(const BSplineTrackDefinition &def);

		template<typename Functor>
		void getCurveSamplingPoints(Functor &&functor) {
			auto timestep = 1.0f / 30.0f;
			auto startFrame = static_cast<int>(floorf(startTime / timestep));
			auto endFrame = static_cast<int>(ceilf(stopTime / timestep));

			for (int frame = startFrame; frame <= endFrame; frame++) {
				auto time = frame * timestep;

				if (time < startTime)
					time = startTime;
				else if (time > stopTime)
					time = stopTime;

				functor(time);
			}
		}

		template<size_t PointSize>
		std::array<float, PointSize> sampleTrack(const std::vector<std::array<float, PointSize>> &track, float time);

		const NIFDictionary &interpolator;
		float startTime;
		float stopTime;
		const NIFDictionary &splineData;
		uint32_t numControlPoints;
		std::vector<int> intervals;

	private:
		void computeIntervals();

		float calculateBlend(int point, float interval, int degree = Degree + 1);
	};

}

#endif
