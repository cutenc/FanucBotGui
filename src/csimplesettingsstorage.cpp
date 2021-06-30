#include "csimplesettingsstorage.h"

#include <QSettings>

static const char *GUI_PREFIX = "GUI";

enum EN_GuiKeys
{
    ENGK_FIRST = 0,

    //Common
    ENGK_MSAA = ENGK_FIRST,

    //The Part
    ENGK_PART_TR_X,
    ENGK_PART_TR_Y,
    ENGK_PART_TR_Z,
    ENGK_PART_CENTER_X,
    ENGK_PART_CENTER_Y,
    ENGK_PART_CENTER_Z,
    ENGK_PART_ROTATE_X,
    ENGK_PART_ROTATE_Y,
    ENGK_PART_ROTATE_Z,
    ENGK_PART_SCALE,
    //The Desk
    ENGK_DESK_TR_X,
    ENGK_DESK_TR_Y,
    ENGK_DESK_TR_Z,
    ENGK_DESK_CENTER_X,
    ENGK_DESK_CENTER_Y,
    ENGK_DESK_CENTER_Z,
    ENGK_DESK_ROTATE_X,
    ENGK_DESK_ROTATE_Y,
    ENGK_DESK_ROTATE_Z,
    ENGK_DESK_SCALE,
    //The Laser Head
    ENGK_LHEAD_TR_X,
    ENGK_LHEAD_TR_Y,
    ENGK_LHEAD_TR_Z,
    ENGK_LHEAD_CENTER_X,
    ENGK_LHEAD_CENTER_Y,
    ENGK_LHEAD_CENTER_Z,
    ENGK_LHEAD_ROTATE_X,
    ENGK_LHEAD_ROTATE_Y,
    ENGK_LHEAD_ROTATE_Z,
    ENGK_LHEAD_SCALE,
    //The Grip
    ENGK_GRIP_TR_X,
    ENGK_GRIP_TR_Y,
    ENGK_GRIP_TR_Z,
    ENGK_GRIP_CENTER_X,
    ENGK_GRIP_CENTER_Y,
    ENGK_GRIP_CENTER_Z,
    ENGK_GRIP_ROTATE_X,
    ENGK_GRIP_ROTATE_Y,
    ENGK_GRIP_ROTATE_Z,
    ENGK_GRIP_SCALE,

    ENGK_LAST
};
typedef int TGuiKey;

static const std::map <TGuiKey, QString> guiKeyMap = {
    { ENGK_MSAA          , "msaa"           },
    //The Part
    { ENGK_PART_TR_X     , "part_tr_x"      },
    { ENGK_PART_TR_Y     , "part_tr_y"      },
    { ENGK_PART_TR_Z     , "part_tr_z"      },
    { ENGK_PART_CENTER_X , "part_center_x"  },
    { ENGK_PART_CENTER_Y , "part_center_y"  },
    { ENGK_PART_CENTER_Z , "part_center_z"  },
    { ENGK_PART_ROTATE_X , "part_rotate_x"  },
    { ENGK_PART_ROTATE_Y , "part_rotate_y"  },
    { ENGK_PART_ROTATE_Z , "part_rotate_z"  },
    { ENGK_PART_SCALE    , "part_scale"     },
    //The Desk
    { ENGK_DESK_TR_X     , "desk_tr_x"      },
    { ENGK_DESK_TR_Y     , "desk_tr_y"      },
    { ENGK_DESK_TR_Z     , "desk_tr_z"      },
    { ENGK_DESK_CENTER_X , "desk_center_x"  },
    { ENGK_DESK_CENTER_Y , "desk_center_y"  },
    { ENGK_DESK_CENTER_Z , "desk_center_z"  },
    { ENGK_DESK_ROTATE_X , "desk_rotate_x"  },
    { ENGK_DESK_ROTATE_Y , "desk_rotate_y"  },
    { ENGK_DESK_ROTATE_Z , "desk_rotate_z"  },
    { ENGK_DESK_SCALE    , "desk_scale"     },
    //The Laser Head
    { ENGK_LHEAD_TR_X     , "lhead_tr_x"      },
    { ENGK_LHEAD_TR_Y     , "lhead_tr_y"      },
    { ENGK_LHEAD_TR_Z     , "lhead_tr_z"      },
    { ENGK_LHEAD_CENTER_X , "lhead_center_x"  },
    { ENGK_LHEAD_CENTER_Y , "lhead_center_y"  },
    { ENGK_LHEAD_CENTER_Z , "lhead_center_z"  },
    { ENGK_LHEAD_ROTATE_X , "lhead_rotate_x"  },
    { ENGK_LHEAD_ROTATE_Y , "lhead_rotate_y"  },
    { ENGK_LHEAD_ROTATE_Z , "lhead_rotate_z"  },
    { ENGK_LHEAD_SCALE    , "lhead_scale"     },
    //The GRIP
    { ENGK_GRIP_TR_X     , "grip_tr_x"      },
    { ENGK_GRIP_TR_Y     , "grip_tr_y"      },
    { ENGK_GRIP_TR_Z     , "grip_tr_z"      },
    { ENGK_GRIP_CENTER_X , "grip_center_x"  },
    { ENGK_GRIP_CENTER_Y , "grip_center_y"  },
    { ENGK_GRIP_CENTER_Z , "grip_center_z"  },
    { ENGK_GRIP_ROTATE_X , "grip_rotate_x"  },
    { ENGK_GRIP_ROTATE_Y , "grip_rotate_y"  },
    { ENGK_GRIP_ROTATE_Z , "grip_rotate_z"  },
    { ENGK_GRIP_SCALE    , "grip_scale"     }
};

class CSimpleSettingsStoragePrivate
{
public:
    CSimpleSettingsStoragePrivate() :
        settingsFile(nullptr)
    { }

    ~CSimpleSettingsStoragePrivate() {
        delete settingsFile;
    }

    void setFName(const char *fname) {
        delete settingsFile;
        settingsFile = new QSettings(QString::fromLocal8Bit(fname), QSettings::IniFormat);
    }

    GUI_TYPES::SGuiSettings loadGuiSettings() {
        using namespace GUI_TYPES;
        SGuiSettings res;
        settingsFile->beginGroup(GUI_PREFIX);
        //Common
        res.msaa          = readGuiValue <TMSAA> (ENGK_MSAA);
        //The Part
        res.partTrX       = readGuiValue <TDistance> (ENGK_PART_TR_X);
        res.partTrY       = readGuiValue <TDistance> (ENGK_PART_TR_Y);
        res.partTrZ       = readGuiValue <TDistance> (ENGK_PART_TR_Z);
        res.partCenterX   = readGuiValue <TDistance> (ENGK_PART_CENTER_X);
        res.partCenterY   = readGuiValue <TDistance> (ENGK_PART_CENTER_Y);
        res.partCenterZ   = readGuiValue <TDistance> (ENGK_PART_CENTER_Z);
        res.partRotationX = readGuiValue <TDegree>   (ENGK_PART_ROTATE_X);
        res.partRotationY = readGuiValue <TDegree>   (ENGK_PART_ROTATE_Y);
        res.partRotationZ = readGuiValue <TDegree>   (ENGK_PART_ROTATE_Z);
        res.partScale     = readGuiValue <TScale>    (ENGK_PART_SCALE);
        //The Desk
        res.deskTrX       = readGuiValue <TDistance> (ENGK_DESK_TR_X);
        res.deskTrY       = readGuiValue <TDistance> (ENGK_DESK_TR_Y);
        res.deskTrZ       = readGuiValue <TDistance> (ENGK_DESK_TR_Z);
        res.deskCenterX   = readGuiValue <TDistance> (ENGK_DESK_CENTER_X);
        res.deskCenterY   = readGuiValue <TDistance> (ENGK_DESK_CENTER_Y);
        res.deskCenterZ   = readGuiValue <TDistance> (ENGK_DESK_CENTER_Z);
        res.deskRotationX = readGuiValue <TDegree>   (ENGK_DESK_ROTATE_X);
        res.deskRotationY = readGuiValue <TDegree>   (ENGK_DESK_ROTATE_Y);
        res.deskRotationZ = readGuiValue <TDegree>   (ENGK_DESK_ROTATE_Z);
        res.deskScale     = readGuiValue <TScale>    (ENGK_DESK_SCALE);
        //The Laser Head
        res.lheadTrX       = readGuiValue <TDistance> (ENGK_LHEAD_TR_X);
        res.lheadTrY       = readGuiValue <TDistance> (ENGK_LHEAD_TR_Y);
        res.lheadTrZ       = readGuiValue <TDistance> (ENGK_LHEAD_TR_Z);
        res.lheadCenterX   = readGuiValue <TDistance> (ENGK_LHEAD_CENTER_X);
        res.lheadCenterY   = readGuiValue <TDistance> (ENGK_LHEAD_CENTER_Y);
        res.lheadCenterZ   = readGuiValue <TDistance> (ENGK_LHEAD_CENTER_Z);
        res.lheadRotationX = readGuiValue <TDegree>   (ENGK_LHEAD_ROTATE_X);
        res.lheadRotationY = readGuiValue <TDegree>   (ENGK_LHEAD_ROTATE_Y);
        res.lheadRotationZ = readGuiValue <TDegree>   (ENGK_LHEAD_ROTATE_Z);
        res.lheadScale     = readGuiValue <TScale>    (ENGK_LHEAD_SCALE);
        //The Grip
        res.gripTrX       = readGuiValue <TDistance> (ENGK_GRIP_TR_X);
        res.gripTrY       = readGuiValue <TDistance> (ENGK_GRIP_TR_Y);
        res.gripTrZ       = readGuiValue <TDistance> (ENGK_GRIP_TR_Z);
        res.gripCenterX   = readGuiValue <TDistance> (ENGK_GRIP_CENTER_X);
        res.gripCenterY   = readGuiValue <TDistance> (ENGK_GRIP_CENTER_Y);
        res.gripCenterZ   = readGuiValue <TDistance> (ENGK_GRIP_CENTER_Z);
        res.gripRotationX = readGuiValue <TDegree>   (ENGK_GRIP_ROTATE_X);
        res.gripRotationY = readGuiValue <TDegree>   (ENGK_GRIP_ROTATE_Y);
        res.gripRotationZ = readGuiValue <TDegree>   (ENGK_GRIP_ROTATE_Z);
        res.gripScale     = readGuiValue <TScale>    (ENGK_GRIP_SCALE);
        settingsFile->endGroup();
        return res;
    }

    void saveGuiSettings(const GUI_TYPES::SGuiSettings &settings) {
        settingsFile->beginGroup(GUI_PREFIX);
        writeGuiValue(ENGK_MSAA         , settings.msaa);
        //The Part
        writeGuiValue(ENGK_PART_TR_X    , settings.partTrX);
        writeGuiValue(ENGK_PART_TR_Y    , settings.partTrY);
        writeGuiValue(ENGK_PART_TR_Z    , settings.partTrZ);
        writeGuiValue(ENGK_PART_CENTER_X, settings.partCenterX);
        writeGuiValue(ENGK_PART_CENTER_Y, settings.partCenterY);
        writeGuiValue(ENGK_PART_CENTER_Z, settings.partCenterZ);
        writeGuiValue(ENGK_PART_ROTATE_X, settings.partRotationX);
        writeGuiValue(ENGK_PART_ROTATE_Y, settings.partRotationY);
        writeGuiValue(ENGK_PART_ROTATE_Z, settings.partRotationZ);
        writeGuiValue(ENGK_PART_SCALE   , settings.partScale);
        //The Desk
        writeGuiValue(ENGK_DESK_TR_X    , settings.deskTrX);
        writeGuiValue(ENGK_DESK_TR_Y    , settings.deskTrY);
        writeGuiValue(ENGK_DESK_TR_Z    , settings.deskTrZ);
        writeGuiValue(ENGK_DESK_CENTER_X, settings.deskCenterX);
        writeGuiValue(ENGK_DESK_CENTER_Y, settings.deskCenterY);
        writeGuiValue(ENGK_DESK_CENTER_Z, settings.deskCenterZ);
        writeGuiValue(ENGK_DESK_ROTATE_X, settings.deskRotationX);
        writeGuiValue(ENGK_DESK_ROTATE_Y, settings.deskRotationY);
        writeGuiValue(ENGK_DESK_ROTATE_Z, settings.deskRotationZ);
        writeGuiValue(ENGK_DESK_SCALE   , settings.deskScale);
        //The Laser Head
        writeGuiValue(ENGK_LHEAD_TR_X    , settings.lheadTrX);
        writeGuiValue(ENGK_LHEAD_TR_Y    , settings.lheadTrY);
        writeGuiValue(ENGK_LHEAD_TR_Z    , settings.lheadTrZ);
        writeGuiValue(ENGK_LHEAD_CENTER_X, settings.lheadCenterX);
        writeGuiValue(ENGK_LHEAD_CENTER_Y, settings.lheadCenterY);
        writeGuiValue(ENGK_LHEAD_CENTER_Z, settings.lheadCenterZ);
        writeGuiValue(ENGK_LHEAD_ROTATE_X, settings.lheadRotationX);
        writeGuiValue(ENGK_LHEAD_ROTATE_Y, settings.lheadRotationY);
        writeGuiValue(ENGK_LHEAD_ROTATE_Z, settings.lheadRotationZ);
        writeGuiValue(ENGK_LHEAD_SCALE   , settings.lheadScale);
        //The Grip
        writeGuiValue(ENGK_GRIP_TR_X    , settings.gripTrX);
        writeGuiValue(ENGK_GRIP_TR_Y    , settings.gripTrY);
        writeGuiValue(ENGK_GRIP_TR_Z    , settings.gripTrZ);
        writeGuiValue(ENGK_GRIP_CENTER_X, settings.gripCenterX);
        writeGuiValue(ENGK_GRIP_CENTER_Y, settings.gripCenterY);
        writeGuiValue(ENGK_GRIP_CENTER_Z, settings.gripCenterZ);
        writeGuiValue(ENGK_GRIP_ROTATE_X, settings.gripRotationX);
        writeGuiValue(ENGK_GRIP_ROTATE_Y, settings.gripRotationY);
        writeGuiValue(ENGK_GRIP_ROTATE_Z, settings.gripRotationZ);
        writeGuiValue(ENGK_GRIP_SCALE   , settings.gripScale);
        settingsFile->endGroup();
    }

private:
    template <typename T>
    T readGuiValue(const TGuiKey key) const {
        T result = T();
        auto it = guiKeyMap.find(key);
        if (it != guiKeyMap.cend()) {
            result = settingsFile->value(it->second).value <T> ();
        }
        return result;
    }

    template <typename T>
    void writeGuiValue(const TGuiKey key, const T value) {
        auto it = guiKeyMap.find(key);
        if (it != guiKeyMap.cend()) {
            settingsFile->setValue(it->second, QVariant::fromValue <T> (value));
        }
    }

private:
    QSettings *settingsFile;
};

CSimpleSettingsStorage::CSimpleSettingsStorage() :
    CAbstractSettingsStorage(),
    d_ptr(new CSimpleSettingsStoragePrivate())
{

}

CSimpleSettingsStorage::~CSimpleSettingsStorage()
{
    delete d_ptr;
}

void CSimpleSettingsStorage::setSettingsFName(const char *fname)
{
    d_ptr->setFName(fname);
}

GUI_TYPES::SGuiSettings CSimpleSettingsStorage::loadGuiSettings()
{
    return d_ptr->loadGuiSettings();
}

void CSimpleSettingsStorage::saveGuiSettings(const GUI_TYPES::SGuiSettings &settings)
{
    d_ptr->saveGuiSettings(settings);
}
