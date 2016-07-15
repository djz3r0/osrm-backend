#include "extractor/guidance/turn_lane_data.hpp"
#include "util/guidance/turn_lanes.hpp"

#include <boost/numeric/conversion/cast.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>

namespace osrm
{
namespace extractor
{
namespace guidance
{
namespace lanes
{

bool TurnLaneData::operator<(const TurnLaneData &other) const
{
    if (from < other.from)
        return true;
    if (from > other.from)
        return false;

    if (to < other.to)
        return true;
    if (to > other.to)
        return false;

    //the suppress-assignment flag is ignored, since it does not influence the order
    const constexpr TurnLaneType::Mask tag_by_modifier[] = {TurnLaneType::sharp_right,
                                                            TurnLaneType::right,
                                                            TurnLaneType::slight_right,
                                                            TurnLaneType::straight,
                                                            TurnLaneType::slight_left,
                                                            TurnLaneType::left,
                                                            TurnLaneType::sharp_left,
                                                            TurnLaneType::uturn};
    return std::find(tag_by_modifier, tag_by_modifier + 8, this->tag) <
           std::find(tag_by_modifier, tag_by_modifier + 8, other.tag);
}

LaneDataVector laneDataFromDescription(TurnLaneDescription turn_lane_description)
{
    typedef std::unordered_map<TurnLaneType::Mask, std::pair<LaneID, LaneID>> LaneMap;
    //TODO need to handle cases that have none-in between two identical values
    const auto num_lanes = boost::numeric_cast<LaneID>(turn_lane_description.size());

    const auto setLaneData = [&](
        LaneMap &map, TurnLaneType::Mask full_mask, const LaneID current_lane) {
        const auto isSet = [&](const TurnLaneType::Mask test_mask) -> bool {
            return (test_mask & full_mask) == test_mask;
        };

        for (std::size_t shift = 0; shift < TurnLaneType::detail::num_supported_lane_types; ++shift)
        {
            TurnLaneType::Mask mask = 1 << shift;
            if (isSet(mask))
            {
                auto map_iterator = map.find(mask);
                if (map_iterator == map.end())
                    map[mask] = std::make_pair(current_lane, current_lane);
                else
                {
                    map_iterator->second.first = current_lane;
                }
            }
        }
    };

    LaneMap lane_map;
    LaneID lane_nr = num_lanes - 1;
    if (turn_lane_description.empty())
        return {};

    for (auto full_mask : turn_lane_description)
    {
        setLaneData(lane_map, full_mask, lane_nr);
        --lane_nr;
    }

    // transform the map into the lane data vector
    LaneDataVector lane_data;
    for (const auto tag : lane_map)
    {
        lane_data.push_back({tag.first, tag.second.first, tag.second.second,false});
    }

    std::sort(lane_data.begin(), lane_data.end());

    // check whether a given turn lane string resulted in valid lane data
    const auto hasValidOverlaps = [](const LaneDataVector &lane_data) {
        // Allow an overlap of at most one. Larger overlaps would result in crossing another turn,
        // which is invalid
        for (std::size_t index = 1; index < lane_data.size(); ++index)
        {
            if (lane_data[index - 1].to > lane_data[index].from)
                return false;
        }
        return true;
    };

    if (!hasValidOverlaps(lane_data))
        lane_data.clear();

    return lane_data;
}

LaneDataVector::iterator findTag(const TurnLaneType::Mask tag, LaneDataVector &data)
{
    return std::find_if(data.begin(), data.end(), [&](const TurnLaneData &lane_data) {
        return (tag & lane_data.tag) != TurnLaneType::empty;
    });
}
LaneDataVector::const_iterator findTag(const TurnLaneType::Mask tag, const LaneDataVector &data)
{
    return std::find_if(data.cbegin(), data.cend(), [&](const TurnLaneData &lane_data) {
        return (tag & lane_data.tag) != TurnLaneType::empty;
    });
}

bool hasTag(const TurnLaneType::Mask tag, const LaneDataVector &data)
{
    return findTag(tag, data) != data.cend();
}

bool isSubsetOf(const LaneDataVector &subset_candidate, const LaneDataVector &superset_candidate)
{
    auto location = superset_candidate.begin();
    for (const auto entry : subset_candidate)
    {
        location =
            std::find_if(location, superset_candidate.end(), [entry](const TurnLaneData &lane_data) {
                return lane_data.tag == entry.tag;
            });

        if (location == superset_candidate.end())
            return false;

        // compare the number of lanes TODO this might have be to be revisited for situations where
        // a sliproad widens into multiple lanes
        if ((location->to - location->from) != (entry.to - entry.from))
            return false;
    }
    return true;
}

} // namespace lanes
} // namespace guidance
} // namespace extractor
} // namespace osrm