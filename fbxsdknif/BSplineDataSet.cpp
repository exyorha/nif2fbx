#include "BSplineDataSet.h"
#include "BSplineTrackDefinition.h"

#include <array>

namespace fbxnif {
	BSplineDataSet::BSplineDataSet(const NIFDictionary &interpolator) :
		interpolator(interpolator),
		startTime(interpolator.getValue<float>("Start Time")),
		stopTime(interpolator.getValue<float>("Stop Time")),
		splineData(std::get<NIFDictionary>(*interpolator.getValue<NIFReference>("Spline Data").ptr)) {

		const auto &basisData = interpolator.getValue<NIFReference>("Basis Data");
		const auto &basisDataDict = std::get<NIFDictionary>(*basisData.ptr);
		numControlPoints = basisDataDict.getValue<uint32_t>("Num Control Points");

		computeIntervals();
	}

	BSplineDataSet::~BSplineDataSet() {

	}

	bool BSplineDataSet::isTrackPresent(const BSplineTrackDefinition &def) const {
		return interpolator.getValue<uint32_t>(def.handleKey) != 65535;
	}


	template<size_t PointSize>
	std::vector<std::array<float, PointSize>> BSplineDataSet::extractTrack(const BSplineTrackDefinition &def) {
		std::vector<std::array<float, PointSize>> result;

		auto handle = interpolator.getValue<uint32_t>(def.handleKey);
		if (handle == 65535)
			return result;

		result.resize(numControlPoints);

		if (interpolator.data.count(def.offsetKey) != 0) {
			auto offset = interpolator.getValue<float>(def.offsetKey);
			auto halfRange = interpolator.getValue<float>(def.halfRangeKey);

			auto numControlPoints = splineData.getValue<uint32_t>("Num Compact Control Points");
			const auto &controlPoints = splineData.getValue<NIFArray>("Compact Control Points");

			if (controlPoints.data.size() < handle + result.size() * PointSize) {
				throw std::runtime_error("Not enough control points");
			}

			for (size_t index = 0, size = result.size(); index < size; index++) {
				for (size_t scalarIndex = 0; scalarIndex < PointSize; scalarIndex++) {
					result[index][scalarIndex] = static_cast<int32_t>(std::get<uint32_t>(controlPoints.data[handle + index * PointSize + scalarIndex])) / 32767.0f * halfRange + offset;
				}
			}
		}
		else {
			auto numControlPoints = splineData.getValue<uint32_t>("Num Control Points");
			const auto &controlPoints = splineData.getValue<NIFArray>("Control Points");

			if (controlPoints.data.size() < handle + result.size() * PointSize) {
				throw std::runtime_error("Not enough control points");
			}

			for (size_t index = 0, size = result.size(); index < size; index++) {
				for (size_t scalarIndex = 0; scalarIndex < PointSize; scalarIndex++) {
					result[index][scalarIndex] = std::get<float>(controlPoints.data[handle + index * PointSize + scalarIndex]);
				}
			}
		}

		return result;
	}

	template<size_t PointSize>
	std::array<float, PointSize> BSplineDataSet::sampleTrack(const std::vector<std::array<float, PointSize>> &track, float time) {
		std::array<float, PointSize> result;
		for (auto &val : result) {
			val = 0.0f;
		}

		float interval = (time - startTime) / (stopTime - startTime) * static_cast<float>(numControlPoints - Degree);

		for (size_t point = 0; point < numControlPoints; point++) {
			float blend;

			if (interval >= static_cast<float>(numControlPoints - BSplineDataSet::Degree)) {
				if (point == numControlPoints - 1) {
					blend = 1.0f;
				}
				else {
					blend = 0.0f;
				}
			}
			else {
				blend = calculateBlend(static_cast<int>(point), interval);
			}

			for (size_t item = 0; item < PointSize; item++) {
				result[item] += track[point][item] * blend;
			}
		}

		return result;
	}

	void BSplineDataSet::computeIntervals() {
		int t = Degree + 1;
		int n = numControlPoints - 1;

		intervals.resize(n + t + 1);

		for (int j = 0; j <= n + t; j++) {
			if (j < t) {
				intervals[j] = 0;
			}
			else if ((t <= j) && (j <= n)) {
				intervals[j] = j - t + 1;
			}
			else if (j > n) {
				intervals[j] = n - t + 2;
			}
		}
	}

	float BSplineDataSet::calculateBlend(int point, float interval, int degree) {
		if (degree == 1) {
			if (intervals[point] <= interval && interval < intervals[point + 1])
				return 1.0f;
			else
				return 0.0f;
		}
		else if (intervals[point + degree - 1] == intervals[point]) {
			if (intervals[point + degree] == intervals[point + 1]) {
				return 0.0f;
			}
			else {
				return (intervals[point + degree] - interval) / (intervals[point + degree] - intervals[point + 1]) * calculateBlend(point + 1, interval, degree - 1);
			}
		}
		else if (intervals[point + degree] == intervals[point + 1]) {
			return (interval - intervals[point]) / (intervals[point + degree - 1] - intervals[point]) * calculateBlend(point, interval, degree - 1);
		}
		else {
			return (interval - intervals[point]) / (intervals[point + degree - 1] - intervals[point]) * calculateBlend(point, interval, degree - 1)
				+ (intervals[point + degree] - interval) / (intervals[point + degree] - intervals[point + 1]) * calculateBlend(point + 1, interval, degree - 1);
		}
	}

	template std::vector<std::array<float, 1>> BSplineDataSet::extractTrack<1>(const BSplineTrackDefinition &def);
	template std::vector<std::array<float, 3>> BSplineDataSet::extractTrack<3>(const BSplineTrackDefinition &def);
	template std::vector<std::array<float, 4>> BSplineDataSet::extractTrack<4>(const BSplineTrackDefinition &def);
	template std::array<float, 1> BSplineDataSet::sampleTrack<1>(const std::vector<std::array<float, 1>> &track, float time);
	template std::array<float, 3> BSplineDataSet::sampleTrack<3>(const std::vector<std::array<float, 3>> &track, float time);
	template std::array<float, 4> BSplineDataSet::sampleTrack<4>(const std::vector<std::array<float, 4>> &track, float time);
}