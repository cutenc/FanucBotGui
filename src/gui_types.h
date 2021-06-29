#ifndef GUI_TYPES_H
#define GUI_TYPES_H

#include <map>

namespace GUI_TYPES
{

enum EN_MSAA
{
    ENMSAA_OFF = 0,
    ENMSAA_2   = 2,
    ENMSAA_4   = 4,
    ENMSAA_8   = 8
};
typedef int TMSAA;

enum EN_CoordSystems
{
    ENCS_RIGHT,
    ENCS_LEFT
};
typedef int TCoordSystem;

enum EN_UserActions
{
    ENUA_NO,
    ENUA_CALIBRATION,
    ENUA_ADD_TASK
};

enum EN_BotTaskTypes
{
    ENBTT_MOVE,
    ENBTT_DRILL,
    ENBTT_MARK
};
typedef int TBotTaskType;

typedef double TDistance;
typedef double TDegree;

struct SVertex
{
    SVertex(const TDistance X = 0., const TDistance Y = 0, const TDistance Z = 0) :
        x(X), y(Y), z(Z) { }

    TDistance x, y, z;
};

struct SRotationAngle
{
    SRotationAngle(const TDegree alpha = 0., const TDegree beta = 0, const TDegree gamma = 0) :
        x(alpha), y(beta), z(gamma) { }

    TDistance x, y, z;
};

struct SCalibPoint
{
    SCalibPoint() {}

    SVertex globalPos, botPos;
};

struct STaskPoint
{
    STaskPoint() : taskType(ENBTT_MOVE) { }

    TBotTaskType taskType;
    SVertex globalPos;
    SRotationAngle angle;
};

template <typename Key, typename Value>
inline static Value extract_map_value(const std::map <Key, Value> &map,
                                      const Key key,
                                      const Value defaultVl = Value()) {
    Value res = defaultVl;
    const typename std::map<Key, Value>::const_iterator it = map.find(key);
    if (it != map.cend())
        res = it->second;
    return res;
}

}

#endif // GUI_TYPES_H
