#include "cmainviewport.h"

#include <vector>
#include <map>

#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QVariant>
#include <QMessageBox>
#include <QDebug>

#include <AIS_ViewController.hxx>

#include <OpenGl_GraphicDriver.hxx>
#include <V3d_Viewer.hxx>
#include <AIS_InteractiveContext.hxx>
#include <V3d_View.hxx>

#include <Graphic3d_ZLayerSettings.hxx>

#include <gp_Quaternion.hxx>
#include <AIS_Trihedron.hxx>
#include <Geom_Axis2Placement.hxx>

#include <AIS_ViewCube.hxx>
#include <AIS_Shape.hxx>
#include <Geom_CartesianPoint.hxx>
#include <AIS_Point.hxx>
#include <AIS_TextLabel.hxx>
#include <TopoDS_Shape.hxx>

#include "cinteractivecontext.h"
#include "caspectwindow.h"
#include "sguisettings.h"

#include "Dialogs/CalibPoints/caddcalibpointdialog.h"
#include "Dialogs/TaskPoints/cbottaskdialogfacade.h"
#include "Dialogs/PathPoints/caddpathpointdialog.h"

#include "cjsonfilepointssaver.h"

static constexpr double DEGREE_K = M_PI / 180.;

static const Quantity_Color BG_CLR   = Quantity_Color( .7765,  .9 , 1.  , Quantity_TOC_RGB);
static const Graphic3d_ZLayerId Z_LAYER_ID_WITHOUT_DEPTH_TEST_DEFAULT = 100;

static const GUI_TYPES::TDistance DISTANCE_PRECITION = 0.000005;
static const GUI_TYPES::TDegree   ROTATION_PRECITION = 0.000005;

static const GUI_TYPES::TScale SNAP_SCALE = 5.;

static const char *backup_points_fname = "_backup_points_.task";

static class CEmptySubscriber : public CAbstractMainViewportSubscriber
{
public:
    CEmptySubscriber() : CAbstractMainViewportSubscriber() { }

protected:
    void uiStateChanged() final { }
    void calibrationChanged() final { }
    void tasksChanged() final { }
    void homePointsChanged() final { }
    void shapeChanged(const GUI_TYPES::EN_ShapeType, const TopoDS_Shape &) final { }
    void transformChanged(const GUI_TYPES::EN_ShapeType, const gp_Trsf &) final { }

} emptySub;



class CMainViewportPrivate : public AIS_ViewController
{
    friend class CMainViewport;

    CMainViewportPrivate(CMainViewport * const qptr) :
        q_ptr(qptr),
        context(new CInteractiveContext()),
        calibResult(BotSocket::ENCR_OK),
        botState(BotSocket::ENBS_FALL) {
        myMouseGestureMap.Clear();
        myMouseGestureMap.Bind(Aspect_VKeyMouse_LeftButton, AIS_MouseGesture_Pan);
        myMouseGestureMap.Bind(Aspect_VKeyMouse_RightButton, AIS_MouseGesture_RotateOrbit);
        SetAllowRotation(Standard_True);
    }

    ~CMainViewportPrivate() {
        delete context;
    }

    void init(OpenGl_GraphicDriver &driver) {
        //Viewer
        viewer = new V3d_Viewer(&driver);
        viewer->SetDefaultViewSize(1000.);
        viewer->SetDefaultViewProj(V3d_XposYposZpos);
        viewer->SetComputedMode(Standard_True);
        viewer->SetDefaultComputedMode(Standard_True);
        viewer->SetDefaultLights();
        viewer->SetLightOn();

        //ZLayer without depth-test
        Graphic3d_ZLayerId zLayerIdWithoutDepthTest = Graphic3d_ZLayerId_UNKNOWN;
        viewer->AddZLayer(zLayerIdWithoutDepthTest);
        Graphic3d_ZLayerSettings zSettings = viewer->ZLayerSettings(zLayerIdWithoutDepthTest);
        zSettings.SetEnableDepthTest(Standard_False);
        viewer->SetZLayerSettings(zLayerIdWithoutDepthTest, zSettings);

        //Context
        AIS_InteractiveContext * const cntxt = new AIS_InteractiveContext(viewer);
        context->setDisableTepthTestZLayer(zLayerIdWithoutDepthTest);
        context->init(*cntxt);
        view = cntxt->CurrentViewer()->CreateView().get();

        //Aspect
        aspect = new CAspectWindow(*q_ptr);
        view->SetWindow(aspect);
        if (!aspect->IsMapped())
            aspect->Map();

        //Final
        view->ChangeRenderingParams().IsAntialiasingEnabled = Standard_True;
        view->SetBackgroundColor(BG_CLR);
        view->MustBeResized();
    }

    static gp_Trsf calc_transform(const gp_Vec &model_translation,
                                  const gp_Vec &model_center,
                                  double scaleFactor,
                                  double alpha_off, double beta_off, double gamma_off,
                                  double alpha_cur = 0.0, double beta_cur = 0.0, double gamma_cur = 0.0) {
        gp_Trsf trsfTr2;
        trsfTr2.SetTranslation(model_translation + model_center);

        gp_Trsf trsfSc;
        if (scaleFactor == 0.)
            scaleFactor = 1.;
        trsfSc.SetScale(gp_Pnt(), scaleFactor);

        gp_Trsf trsfRcur;
        gp_Quaternion qcur;
        qcur.SetEulerAngles(gp_Extrinsic_XYZ,
                         alpha_cur * DEGREE_K,
                         beta_cur  * DEGREE_K,
                         gamma_cur * DEGREE_K);
        trsfRcur.SetRotation(qcur);

        gp_Trsf trsfRoff;
        gp_Quaternion qoff;
        qoff.SetEulerAngles(gp_Extrinsic_XYZ,
                         alpha_off * DEGREE_K,
                         beta_off  * DEGREE_K,
                         gamma_off * DEGREE_K);
        trsfRoff.SetRotation(qoff);

        gp_Trsf trsfTr1;
        trsfTr1.SetTranslation(-model_center);

        return trsfTr2 * trsfSc * trsfRcur * trsfRoff * trsfTr1;
    }

    gp_Trsf calcPartTrsf() const {
        return calc_transform(gp_Vec(guiSettings.partTrX + partPos.globalPos.x,
                                     guiSettings.partTrY + partPos.globalPos.y,
                                     guiSettings.partTrZ + partPos.globalPos.z),
                              gp_Vec(guiSettings.partCenterX,
                                     guiSettings.partCenterY,
                                     guiSettings.partCenterZ),
                              guiSettings.partScale,
                              guiSettings.partRotationX,
                              guiSettings.partRotationY,
                              guiSettings.partRotationZ,
                              partPos.globalRotation.x,
                              partPos.globalRotation.y,
                              partPos.globalRotation.z);

    }

    gp_Trsf calcDeskTrsf() const {
        return calc_transform(gp_Vec(guiSettings.deskTrX,
                                     guiSettings.deskTrY,
                                     guiSettings.deskTrZ),
                              gp_Vec(guiSettings.deskCenterX,
                                     guiSettings.deskCenterY,
                                     guiSettings.deskCenterZ),
                              guiSettings.deskScale,
                              guiSettings.deskRotationX,
                              guiSettings.deskRotationY,
                              guiSettings.deskRotationZ);
    }

    gp_Trsf calcLsrheadTrsf() const {
        return calc_transform(gp_Vec(guiSettings.lheadTrX + lheadPos.globalPos.x,
                                     guiSettings.lheadTrY + lheadPos.globalPos.y,
                                     guiSettings.lheadTrZ + lheadPos.globalPos.z),
                              gp_Vec(guiSettings.lheadCenterX,
                                     guiSettings.lheadCenterY,
                                     guiSettings.lheadCenterZ),
                              guiSettings.lheadScale,
                              guiSettings.lheadRotationX,
                              guiSettings.lheadRotationY,
                              guiSettings.lheadRotationZ,
                              lheadPos.globalRotation.x,
                              lheadPos.globalRotation.y,
                              lheadPos.globalRotation.z);
    }

    gp_Trsf calcGripTrsf() const {
        return calc_transform(gp_Vec(guiSettings.gripTrX + gripPos.globalPos.x,
                                     guiSettings.gripTrY + gripPos.globalPos.y,
                                     guiSettings.gripTrZ + gripPos.globalPos.z),
                              gp_Vec(guiSettings.gripCenterX,
                                     guiSettings.gripCenterY,
                                     guiSettings.gripCenterZ),
                              guiSettings.gripScale,
                              guiSettings.gripRotationX,
                              guiSettings.gripRotationY,
                              guiSettings.gripRotationZ,
                              gripPos.globalRotation.x,
                              gripPos.globalRotation.y,
                              gripPos.globalRotation.z);
    }

    template <typename T>
    inline static void translatePoints(std::vector <T> &vec, const gp_Trsf trsf) {
        for(auto &spnt : vec) {
            gp_Pnt pnt(spnt.globalPos.x, spnt.globalPos.y, spnt.globalPos.z);
            pnt.Transform(trsf);
            spnt.globalPos.x = pnt.X();
            spnt.globalPos.y = pnt.Y();
            spnt.globalPos.z = pnt.Z();
        }
    }

    void setGuiSettings(const GUI_TYPES::SGuiSettings &settings) {
        const gp_Trsf oldPartTr = calcPartTrsf();
        guiSettings = settings;
        view->ChangeRenderingParams().NbMsaaSamples = settings.msaa;
        if (guiSettings.snapshotScale == 0.)
            guiSettings.snapshotScale = SNAP_SCALE;

        //Part and Points
        const gp_Trsf newPartTr = calcPartTrsf();
        const gp_Trsf pointsDeltaTr = newPartTr * oldPartTr.Inverted();
        std::vector <GUI_TYPES::SCalibPoint> calibVec = getCallibrationPoints();
        translatePoints(calibVec, pointsDeltaTr);
        setCalibrationPoints(calibVec);
        std::vector <GUI_TYPES::STaskPoint> taskVec = getTaskPoints();
        translatePoints(taskVec, pointsDeltaTr);
        setTaskPoints(taskVec);
        context->setPartMdlTransform(newPartTr);

        //Desk
        context->setDeskMdlTransform(calcDeskTrsf());
        const gp_Pnt start = gp_Pnt(guiSettings.lheadLsrTrX,
                                    guiSettings.lheadLsrTrY,
                                    guiSettings.lheadLsrTrZ);
        gp_Dir dir;
        if (guiSettings.lheadLsrNormalX != 0. ||
                guiSettings.lheadLsrNormalY != 0. ||
                guiSettings.lheadLsrNormalZ != 0.)
            dir = gp_Dir(guiSettings.lheadLsrNormalX,
                         guiSettings.lheadLsrNormalY,
                         guiSettings.lheadLsrNormalZ);
        context->setLaserLine(start, dir, guiSettings.lheadLsrLenght, guiSettings.lheadLsrClip);
        context->setLsrheadMdlTransform(calcLsrheadTrsf());
        context->setGripMdlTransform(calcGripTrsf());
        context->setGripVisible(guiSettings.gripVis);
        view->Redraw();
    }

    void setPartModel(const TopoDS_Shape &shape) {
        context->setPartModel(shape);
        context->setPartMdlTransform(calcPartTrsf());
        view->Redraw();
    }

    void setDeskModel(const TopoDS_Shape &shape) {
        context->setDeskModel(shape);
        context->setDeskMdlTransform(calcDeskTrsf());
        view->Redraw();
    }

    void setLsrheadModel(const TopoDS_Shape &shape) {
        context->setLsrheadModel(shape);
        context->setLsrheadMdlTransform(calcLsrheadTrsf());
        view->Redraw();
    }

    void setGripModel(const TopoDS_Shape &shape) {
        context->setGripModel(shape);
        context->setGripMdlTransform(calcGripTrsf());
        context->setGripVisible(guiSettings.gripVis);
        view->Redraw();
    }

    void setMSAA(const GUI_TYPES::TMSAA msaa) {
        assert(!view.IsNull());
        guiSettings.msaa = msaa;
        view->ChangeRenderingParams().NbMsaaSamples = msaa;
        view->Redraw();
    }

    void setShading(const bool enabled) {
        context->setShading(enabled);
        view->Redraw();
    }

    void setUiState(const GUI_TYPES::EN_UiStates state) {
        context->setUiState(state);
        context->setGripVisible(guiSettings.gripVis);
        view->Redraw();
    }

    void moveLsrhead(const BotSocket::SBotPosition &pos) {
        if (!pos.isEqual(lheadPos, DISTANCE_PRECITION, ROTATION_PRECITION)) {
            lheadPos = pos;
            context->setLsrheadMdlTransform(calcLsrheadTrsf());
            view->Redraw();
        }
    }

    void moveGrip(const BotSocket::SBotPosition &pos) {
        gripPos = pos;
        context->setGripMdlTransform(calcGripTrsf());
        if (botState == BotSocket::ENBS_ATTACHED) {
            partPos = pos;
            context->setPartMdlTransform(calcPartTrsf());
        }
        view->Redraw();
    }

    template <typename TPoint>
    static void updatePntTransform(TPoint &pnt, const gp_Trsf &transform) {
        gp_Pnt gpPnt(pnt.globalPos.x, pnt.globalPos.y, pnt.globalPos.z);
        gpPnt.Transform(transform);
        pnt.globalPos.x = gpPnt.X();
        pnt.globalPos.y = gpPnt.Y();
        pnt.globalPos.z = gpPnt.Z();
    }

    void shapeCalibrationChanged(const GUI_TYPES::EN_ShapeType shType, const BotSocket::SBotPosition &pos)
    {
        using namespace GUI_TYPES;
        switch(shType) {
            case ENST_DESK   : {
                guiSettings.deskTrX = pos.globalPos.x;
                guiSettings.deskTrY = pos.globalPos.y;
                guiSettings.deskTrZ = pos.globalPos.z;
                guiSettings.deskRotationX = pos.globalRotation.x;
                guiSettings.deskRotationY = pos.globalRotation.y;
                guiSettings.deskRotationZ = pos.globalRotation.z;
                context->setDeskMdlTransform(calcDeskTrsf());
                view->Redraw();
                break;
            }
            case ENST_PART   : {
                guiSettings.partTrX = pos.globalPos.x;
                guiSettings.partTrY = pos.globalPos.y;
                guiSettings.partTrZ = pos.globalPos.z;
                guiSettings.partRotationX = pos.globalRotation.x;
                guiSettings.partRotationY = pos.globalRotation.y;
                guiSettings.partRotationZ = pos.globalRotation.z;
                const gp_Trsf newTrsf = calcPartTrsf();
                const gp_Trsf pntTrsf = context->getTransform(GUI_TYPES::ENST_PART).Inverted() * newTrsf;
                context->setPartMdlTransform(newTrsf);
                //points transformation by new calibration data
                const size_t calibPntCount = context->getCalibPointCount();
                for(size_t i = 0; i < calibPntCount; ++i) {
                    GUI_TYPES::SCalibPoint pnt = context->getCalibPoint(i);
                    updatePntTransform(pnt, pntTrsf);
                    context->changeCalibPoint(i, pnt);
                }
                const size_t taskPntCount = context->getTaskPointCount();
                for(size_t i = 0; i < taskPntCount; ++i) {
                    GUI_TYPES::STaskPoint pnt = context->getTaskPoint(i);
                    updatePntTransform(pnt, pntTrsf);
                    context->changeTaskPoint(i, pnt);
                }
                const size_t homePntCount = context->getHomePointCount();
                for(size_t i = 0; i < homePntCount; ++i) {
                    GUI_TYPES::SHomePoint pnt = context->getHomePoint(i);
                    updatePntTransform(pnt, pntTrsf);
                    context->changeHomePoint(i, pnt);
                }
                view->Redraw();
                break;
            }
            case ENST_LSRHEAD: {
                guiSettings.lheadTrX = pos.globalPos.x;
                guiSettings.lheadTrY = pos.globalPos.y;
                guiSettings.lheadTrZ = pos.globalPos.z;
                guiSettings.lheadRotationX = pos.globalRotation.x;
                guiSettings.lheadRotationY = pos.globalRotation.y;
                guiSettings.lheadRotationZ = pos.globalRotation.z;
                context->setLsrheadMdlTransform(calcLsrheadTrsf());
                view->Redraw();
                break;
            }
            case ENST_GRIP   : {
                guiSettings.gripTrX = pos.globalPos.x;
                guiSettings.gripTrY = pos.globalPos.y;
                guiSettings.gripTrZ = pos.globalPos.z;
                guiSettings.gripRotationX = pos.globalRotation.x;
                guiSettings.gripRotationY = pos.globalRotation.y;
                guiSettings.gripRotationZ = pos.globalRotation.z;
                context->setGripMdlTransform(calcGripTrsf());
                view->Redraw();
                break;
            }
            default: break;
        }
    }

    void shapeTransformChanged(const GUI_TYPES::EN_ShapeType shType, const gp_Trsf &transform)
    {
        using namespace GUI_TYPES;
        switch(shType) {
            case ENST_DESK   : {
                context->setDeskMdlTransform(transform);
                view->Redraw();
                break;
            }
            case ENST_PART   : {
                context->setPartMdlTransform(transform);
                view->Redraw();
                break;
            }
            case ENST_LSRHEAD: {
                context->setLsrheadMdlTransform(transform);
                view->Redraw();
                break;
            }
            case ENST_GRIP   : {
                context->setGripMdlTransform(transform);
                view->Redraw();
                break;
            }
            default: break;
        }
    }

    void setCalibrationPoints(const std::vector<GUI_TYPES::SCalibPoint> &points) {
        while(context->getCalibPointCount() > 0)
            context->removeCalibPoint(0);
        for (const auto &pnt : points)
            context->appendCalibPoint(pnt);
        view->Redraw();
    }

    std::vector<GUI_TYPES::SCalibPoint> getCallibrationPoints() const {
        std::vector <GUI_TYPES::SCalibPoint> res;
        const size_t count = context->getCalibPointCount();
        for(size_t i = 0; i < count; ++i)
            res.push_back(context->getCalibPoint(i));
        return res;
    }

    void setTaskPoints(const std::vector <GUI_TYPES::STaskPoint> &points) {
        while(context->getTaskPointCount() > 0)
            context->removeTaskPoint(0);
        for (const auto &pnt : points)
            context->appendTaskPoint(pnt);
        view->Redraw();
    }

    std::vector <GUI_TYPES::STaskPoint> getTaskPoints() const {
        std::vector <GUI_TYPES::STaskPoint> res;
        const size_t count = context->getTaskPointCount();
        for(size_t i = 0; i < count; ++i)
            res.push_back(context->getTaskPoint(i));
        return res;
    }

    CMainViewport * const q_ptr;
    std::vector <CAbstractMainViewportSubscriber *> subs;

    Handle(V3d_Viewer)             viewer;
    Handle(V3d_View)               view;
    Handle(CAspectWindow)          aspect;

    GUI_TYPES::SGuiSettings guiSettings;
    CInteractiveContext * const context;

    QPoint rbPos;

    BotSocket::EN_CalibResult calibResult;
    BotSocket::EN_BotState botState;
    BotSocket::SBotPosition partPos, lheadPos, gripPos;
};



CMainViewport::CMainViewport(QWidget *parent) :
    QWidget(parent),
    d_ptr(new CMainViewportPrivate(this))
{
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
    setBackgroundRole(QPalette::NoRole);
    setFocusPolicy(Qt::StrongFocus);
}

CMainViewport::~CMainViewport()
{
    delete d_ptr;
}

void CMainViewport::addSubscriber(CAbstractMainViewportSubscriber * const subscriber)
{
    d_ptr->subs.push_back(subscriber);
    subscriber->calibrationChanged();
}

void CMainViewport::clearSubscribers()
{
    d_ptr->subs.clear();
}

void CMainViewport::init(OpenGl_GraphicDriver &driver)
{
    d_ptr->init(driver);
}

void CMainViewport::setGuiSettings(const GUI_TYPES::SGuiSettings &settings)
{
    d_ptr->setGuiSettings(settings);
    for(auto s : d_ptr->subs) {
        using namespace GUI_TYPES;
        s->transformChanged(ENST_DESK, d_ptr->context->getTransform(ENST_DESK));
        s->transformChanged(ENST_PART, d_ptr->context->getTransform(ENST_PART));
        s->transformChanged(ENST_GRIP, d_ptr->context->getTransform(ENST_GRIP));
        s->transformChanged(ENST_LSRHEAD, d_ptr->context->getTransform(ENST_LSRHEAD));
    }
}

GUI_TYPES::SGuiSettings CMainViewport::getGuiSettings() const
{
    return d_ptr->guiSettings;
}

void CMainViewport::setMSAA(const GUI_TYPES::TMSAA msaa)
{
    d_ptr->setMSAA(msaa);
}

GUI_TYPES::TMSAA CMainViewport::getMSAA() const
{
    return static_cast <GUI_TYPES::TMSAA> (d_ptr->view->RenderingParams().NbMsaaSamples);
}

GUI_TYPES::TScale CMainViewport::getSnapshotScale() const
{
    return d_ptr->guiSettings.snapshotScale;
}

void CMainViewport::setStatsVisible(const bool value)
{
    d_ptr->view->ChangeRenderingParams().ToShowStats = value;
}

void CMainViewport::setShading(const bool enabled)
{
    d_ptr->setShading(enabled);
}

void CMainViewport::fitInView()
{
    d_ptr->view->FitAll();
    d_ptr->view->ZFitAll();
    d_ptr->view->Redraw();
}

void CMainViewport::setCoord(const GUI_TYPES::TCoordSystem type)
{
    V3d_TypeOfOrientation orientation = V3d_XposYnegZpos;
    if (type == GUI_TYPES::ENCS_LEFT)
        orientation = V3d_XposYnegZneg;
    d_ptr->view->SetProj(orientation, Standard_False);
    d_ptr->view->Redraw();
}

void CMainViewport::setUiState(const GUI_TYPES::EN_UiStates state)
{
    d_ptr->setUiState(state);
    for(auto s : d_ptr->subs)
        s->uiStateChanged();
}

GUI_TYPES::EN_UiStates CMainViewport::getUiState() const
{
    return d_ptr->context->uiState();
}

void CMainViewport::setPartModel(const TopoDS_Shape &shape)
{
    d_ptr->setPartModel(shape);
    for(auto s : d_ptr->subs) {
        s->shapeChanged(GUI_TYPES::ENST_PART, shape);
        s->transformChanged(GUI_TYPES::ENST_PART, d_ptr->context->getTransform(GUI_TYPES::ENST_PART));
    }
}

void CMainViewport::setDeskModel(const TopoDS_Shape &shape)
{
    d_ptr->setDeskModel(shape);
    for(auto s : d_ptr->subs) {
        s->shapeChanged(GUI_TYPES::ENST_DESK, shape);
        s->transformChanged(GUI_TYPES::ENST_DESK, d_ptr->context->getTransform(GUI_TYPES::ENST_DESK));
    }
}

void CMainViewport::setLsrheadModel(const TopoDS_Shape &shape)
{
    d_ptr->setLsrheadModel(shape);
    for(auto s : d_ptr->subs) {
        s->shapeChanged(GUI_TYPES::ENST_LSRHEAD, shape);
        s->transformChanged(GUI_TYPES::ENST_LSRHEAD, d_ptr->context->getTransform(GUI_TYPES::ENST_LSRHEAD));
    }
}

void CMainViewport::setGripModel(const TopoDS_Shape &shape)
{
    d_ptr->setGripModel(shape);
    for(auto s : d_ptr->subs) {
        s->shapeChanged(GUI_TYPES::ENST_GRIP, shape);
        s->transformChanged(GUI_TYPES::ENST_GRIP, d_ptr->context->getTransform(GUI_TYPES::ENST_GRIP));
    }
}

const TopoDS_Shape& CMainViewport::getPartShape() const
{
    return d_ptr->context->getPartShape();
}

const TopoDS_Shape& CMainViewport::getDeskShape() const
{
    return d_ptr->context->getDeskShape();
}

const TopoDS_Shape& CMainViewport::getLsrheadShape() const
{
    return d_ptr->context->getLsrHeadShape();
}

const TopoDS_Shape& CMainViewport::getGripShape() const
{
    return d_ptr->context->getGripShape();
}

const gp_Trsf CMainViewport::getTransform(const GUI_TYPES::EN_ShapeType shType) const
{
    return d_ptr->context->getTransform(shType);
}

void CMainViewport::setCalibResult(const BotSocket::EN_CalibResult val)
{
    if (val != d_ptr->calibResult)
    {
        d_ptr->calibResult = val;
        for(auto s : d_ptr->subs)
            s->calibrationChanged();
    }
}

BotSocket::EN_CalibResult CMainViewport::getCalibResult() const
{
    return d_ptr->calibResult;
}

void CMainViewport::setBotState(const BotSocket::EN_BotState state)
{
    d_ptr->botState = state;
}

BotSocket::EN_BotState CMainViewport::getBotState() const
{
    return d_ptr->botState;
}

void CMainViewport::moveLsrhead(const BotSocket::SBotPosition &pos)
{
    d_ptr->moveLsrhead(pos);
}

void CMainViewport::moveGrip(const BotSocket::SBotPosition &pos)
{
    d_ptr->moveGrip(pos);
    for(auto s : d_ptr->subs) {
        s->transformChanged(GUI_TYPES::ENST_GRIP, d_ptr->context->getTransform(GUI_TYPES::ENST_GRIP));
        if (d_ptr->botState == BotSocket::ENBS_ATTACHED)
            s->transformChanged(GUI_TYPES::ENST_PART, d_ptr->context->getTransform(GUI_TYPES::ENST_PART));
    }
}

void CMainViewport::shapeCalibrationChanged(const GUI_TYPES::EN_ShapeType shType, const BotSocket::SBotPosition &pos)
{
    d_ptr->shapeCalibrationChanged(shType, pos);
    for(auto s : d_ptr->subs)
        s->transformChanged(shType, d_ptr->context->getTransform(shType));
}

void CMainViewport::shapeTransformChanged(const GUI_TYPES::EN_ShapeType shType, const gp_Trsf &transform)
{
    d_ptr->shapeTransformChanged(shType, transform);
    for(auto s : d_ptr->subs)
        s->transformChanged(shType, transform);
}

void CMainViewport::setCalibrationPoints(const std::vector<GUI_TYPES::SCalibPoint> &points)
{
    d_ptr->setCalibrationPoints(points);
}

std::vector<GUI_TYPES::SCalibPoint> CMainViewport::getCallibrationPoints() const
{
    return d_ptr->getCallibrationPoints();
}

std::vector<GUI_TYPES::SCalibPoint> CMainViewport::getCallibrationLocalPoints() const
{
    std::vector <GUI_TYPES::SCalibPoint> res;
    const size_t count = d_ptr->context->getCalibPointCount();
    for(size_t i = 0; i < count; ++i)
        res.push_back(d_ptr->context->getCalibLocalPoint(i));
    return res;
}

void CMainViewport::setTaskPoints(const std::vector<GUI_TYPES::STaskPoint> &points)
{
    d_ptr->setTaskPoints(points);
    taskPointsChanged();
}

std::vector<GUI_TYPES::STaskPoint> CMainViewport::getTaskPoints() const
{
    return d_ptr->getTaskPoints();
}

void CMainViewport::setHomePoints(const std::vector<GUI_TYPES::SHomePoint> &points)
{
    // do not remove current home point if task file doesn't contains any
    if(!points.empty())
    {
        while(d_ptr->context->getHomePointCount() > 0)
            d_ptr->context->removeHomePoint(0);
    }
    for (const auto &pnt : points)
        d_ptr->context->appendHomePoint(pnt);
    d_ptr->viewer->Redraw();
    homePointsChanged();
}

std::vector<GUI_TYPES::SHomePoint> CMainViewport::getHomePoints() const
{
    std::vector <GUI_TYPES::SHomePoint> res;
    const size_t count = d_ptr->context->getHomePointCount();
    for(size_t i = 0; i < count; ++i)
        res.push_back(d_ptr->context->getHomePoint(i));
    return res;
}

template <typename T>
inline static std::vector <T> movedPoints(const std::vector <T> points,
                                          const gp_Vec &globalDelta)
{
    std::vector <T> result(points);
    for(auto &p : result)
    {
        p.globalPos.x += globalDelta.X();
        p.globalPos.y += globalDelta.Y();
        p.globalPos.z += globalDelta.Z();
    }
    return result;
}

void CMainViewport::makeCorrectionBySnapshot(const gp_Vec &globalDelta)
{
    //Part correction
    d_ptr->guiSettings.partTrX += globalDelta.X();
    d_ptr->guiSettings.partTrY += globalDelta.Y();
    d_ptr->guiSettings.partTrZ += globalDelta.Z();
    d_ptr->context->setPartMdlTransform(d_ptr->calcPartTrsf());
    for(auto s : d_ptr->subs)
        s->transformChanged(GUI_TYPES::ENST_PART, d_ptr->context->getTransform(GUI_TYPES::ENST_PART));

    //Points correction
    setCalibrationPoints(movedPoints(getCallibrationLocalPoints(), globalDelta));
    setTaskPoints(movedPoints(getTaskPoints(), globalDelta));
    setHomePoints(movedPoints(getHomePoints(), globalDelta));

    emit updateGuiSettings();
}

QPaintEngine *CMainViewport::paintEngine() const
{
    return nullptr;
}

void CMainViewport::paintEvent(QPaintEvent *)
{
    d_ptr->view->InvalidateImmediate();
    d_ptr->FlushViewEvents(&d_ptr->context->context(), d_ptr->view, Standard_True);
}

void CMainViewport::resizeEvent(QResizeEvent *)
{
    d_ptr->view->MustBeResized();
}

//! Map Qt buttons bitmask to virtual keys.
inline static Aspect_VKeyMouse qtMouseButtons2VKeys(Qt::MouseButtons theButtons)
{
    Aspect_VKeyMouse aButtons = Aspect_VKeyMouse_NONE;
    if ((theButtons & Qt::LeftButton) != 0)
    {
        aButtons |= Aspect_VKeyMouse_LeftButton;
    }
    if ((theButtons & Qt::MiddleButton) != 0)
    {
        aButtons |= Aspect_VKeyMouse_MiddleButton;
    }
    if ((theButtons & Qt::RightButton) != 0)
    {
        aButtons |= Aspect_VKeyMouse_RightButton;
    }
    return aButtons;
}

//! Map Qt mouse modifiers bitmask to virtual keys.
inline static Aspect_VKeyFlags qtMouseModifiers2VKeys(Qt::KeyboardModifiers theModifiers)
{
    Aspect_VKeyFlags aFlags = Aspect_VKeyFlags_NONE;
    if ((theModifiers & Qt::ShiftModifier) != 0)
    {
        aFlags |= Aspect_VKeyFlags_SHIFT;
    }
    if ((theModifiers & Qt::ControlModifier) != 0)
    {
        aFlags |= Aspect_VKeyFlags_CTRL;
    }
    if ((theModifiers & Qt::AltModifier) != 0)
    {
        aFlags |= Aspect_VKeyFlags_ALT;
    }
    return aFlags;
}

void CMainViewport::mousePressEvent(QMouseEvent *event)
{
    const Graphic3d_Vec2i aPnt(event->pos().x(), event->pos().y());
    const Aspect_VKeyFlags aFlags = qtMouseModifiers2VKeys(event->modifiers());
    if (d_ptr->UpdateMouseButtons(aPnt, qtMouseButtons2VKeys(event->buttons()), aFlags, false))
        update();

    if ((event->buttons() & Qt::RightButton) == Qt::RightButton)
        d_ptr->rbPos = event->globalPos();
}

void CMainViewport::mouseReleaseEvent(QMouseEvent *event)
{
    const Graphic3d_Vec2i aPnt(event->pos().x(), event->pos().y());
    const Aspect_VKeyFlags aFlags = qtMouseModifiers2VKeys(event->modifiers());
    if (d_ptr->UpdateMouseButtons(aPnt, qtMouseButtons2VKeys(event->buttons()), aFlags, false))
        update();

    if (!d_ptr->rbPos.isNull()) {
        d_ptr->rbPos = QPoint();

        QMenu menu;
        switch(d_ptr->context->uiState())
        {
            using namespace GUI_TYPES;

            case ENUS_CALIBRATION:
                fillCalibCntxtMenu(menu);
                break;

            case ENUS_TASK_EDITING:
                fillTaskAddCntxtMenu(menu);
                break;

            default:
                break;
        }

        if (!menu.isEmpty())
            menu.exec(event->globalPos());
    }
}

void CMainViewport::mouseMoveEvent(QMouseEvent *event)
{
    const Graphic3d_Vec2i aNewPos(event->pos().x(), event->pos().y());
    if (d_ptr->UpdateMousePosition(aNewPos,
                                   qtMouseButtons2VKeys(event->buttons()),
                                   qtMouseModifiers2VKeys(event->modifiers()),
                                   false))
    {
        update();
    }

    d_ptr->rbPos = QPoint();
    switch(d_ptr->context->uiState())
    {
        case GUI_TYPES::ENUS_CALIBRATION:
        case GUI_TYPES::ENUS_TASK_EDITING:
            d_ptr->context->updateCursorPosition();
            d_ptr->view->Redraw();
            break;
        default:
            break;
    }
}

void CMainViewport::wheelEvent(QWheelEvent *event)
{
    const Graphic3d_Vec2i aPos(event->pos().x(), event->pos().y());
    if (d_ptr->UpdateZoom(Aspect_ScrollDelta(aPos, event->delta() / 8)))
        update();
}

QString CMainViewport::taskName(const GUI_TYPES::TBotTaskType taskType) const
{
    using namespace GUI_TYPES;
    const std::map <TBotTaskType, QString> mapNames = {
        { ENBTT_MOVE , tr("Перемещение") },
        { ENBTT_DRILL, tr("Отверстие")   },
        { ENBTT_MARK , tr("Маркировка")  }
    };
    return extract_map_value(mapNames, taskType, QString());
}

void CMainViewport::fillCalibCntxtMenu(QMenu &menu)
{
    if (d_ptr->context->isPartDetected())
    {
        menu.addAction(CMainViewport::tr("Точка привязки"),
                       this,
                       &CMainViewport::slAddCalibPoint);
    }
    else
    {
        size_t index = 0;
        if (d_ptr->context->isCalibPointDetected(index))
        {
            menu.addAction(CMainViewport::tr("Изменить"),
                           this,
                           &CMainViewport::slChangeCalibPoint)->
                    setProperty("index", static_cast <qulonglong> (index));
            menu.addAction(CMainViewport::tr("Удалить"),
                           this,
                           &CMainViewport::slRemoveCalibPoint)->
                    setProperty("index", static_cast <qulonglong> (index));;
        }
    }
}

void CMainViewport::fillTaskAddCntxtMenu(QMenu &menu)
{
    if (d_ptr->context->isPartDetected())
    {
        using namespace GUI_TYPES;
        menu.addAction(taskName(ENBTT_MOVE),
                       this,
                       &CMainViewport::slAddTaskPoint)->setProperty("taskType", ENBTT_MOVE);
        menu.addAction(taskName(ENBTT_DRILL),
                       this,
                       &CMainViewport::slAddTaskPoint)->setProperty("taskType", ENBTT_DRILL);
        menu.addAction(taskName(ENBTT_MARK),
                       this,
                       &CMainViewport::slAddTaskPoint)->setProperty("taskType", ENBTT_MARK);
        menu.addSeparator();
        menu.addAction(tr("Домашняя точка"),
                       this,
                       &CMainViewport::slAddPathPoint);
    }
    else if(d_ptr->context->isDeskDetected()) {
        menu.addAction(taskName(GUI_TYPES::ENBTT_MOVE),
                       this,
                       &CMainViewport::slAddTaskPoint)->setProperty("taskType", GUI_TYPES::ENBTT_MOVE);
        menu.addSeparator();
        menu.addAction(tr("Домашняя точка"),
                       this,
                       &CMainViewport::slAddPathPoint);
    }
    else if (d_ptr->context->isGripDetected() || d_ptr->context->isLsrheadDetected()) {
        menu.addAction(tr("Домашняя точка"),
                       this,
                       &CMainViewport::slAddPathPoint);
    }
    else {
        size_t index = 0;
        if (d_ptr->context->isTaskPointDetected(index))
        {
                menu.addAction(CMainViewport::tr("Изменить"),
                               this,
                               &CMainViewport::slChangeTaskPoint)->
                        setProperty("index", static_cast <qulonglong> (index));
                menu.addAction(CMainViewport::tr("Удалить"),
                               this,
                               &CMainViewport::slRemoveTaskPoint)->
                        setProperty("index", static_cast <qulonglong> (index));;
        }
        else if (d_ptr->context->isPathPointDetected(index))
        {
                menu.addAction(CMainViewport::tr("Изменить"),
                               this,
                               &CMainViewport::slChangePathPoint)->
                        setProperty("index", static_cast <qulonglong> (index));
                menu.addAction(CMainViewport::tr("Удалить"),
                               this,
                               &CMainViewport::slRemovePathPoint)->
                        setProperty("index", static_cast <qulonglong> (index));;
        }
    }
}

void CMainViewport::taskPointsChanged()
{
    savePoints(backup_points_fname);
    for(auto s : d_ptr->subs)
        s->tasksChanged();
}

void CMainViewport::homePointsChanged()
{
    savePoints(backup_points_fname);
    for(auto s : d_ptr->subs)
        s->homePointsChanged();
}

void CMainViewport::slAddCalibPoint()
{
    const gp_Pnt cursorPos = d_ptr->context->lastCursorPosition();
    GUI_TYPES::SCalibPoint initPoint;
    initPoint.globalPos.x = cursorPos.X();
    initPoint.globalPos.y = cursorPos.Y();
    initPoint.globalPos.z = cursorPos.Z();
    initPoint.botPos      = d_ptr->lheadPos.globalPos;
    CAddCalibPointDialog dialog(this, initPoint);
    if (dialog.exec() == QDialog::Accepted)
    {
        setCalibResult(BotSocket::ENCR_FALL);
        d_ptr->context->appendCalibPoint(dialog.getCalibPoint());
        d_ptr->viewer->Redraw();
    }
}

void CMainViewport::slChangeCalibPoint()
{
    assert(sender() != nullptr);

    const size_t index = static_cast <size_t> (sender()->property("index").toULongLong());
    if (index < d_ptr->context->getCalibPointCount())
    {
        const GUI_TYPES::SCalibPoint calibPnt = d_ptr->context->getCalibPoint(index);
        CAddCalibPointDialog dialog(this, calibPnt);
        if (dialog.exec() == QDialog::Accepted)
        {
            setCalibResult(BotSocket::ENCR_FALL);
            d_ptr->context->changeCalibPoint(index, dialog.getCalibPoint());
            d_ptr->viewer->Redraw();
        }
    }
}

void CMainViewport::slRemoveCalibPoint()
{
    assert(sender() != nullptr);

    const size_t index = static_cast <size_t> (sender()->property("index").toULongLong());
    if (index < d_ptr->context->getCalibPointCount())
    {
        setCalibResult(BotSocket::ENCR_FALL);
        d_ptr->context->removeCalibPoint(index);
        d_ptr->viewer->Redraw();
    }
}

void CMainViewport::slAddTaskPoint()
{
    assert(sender() != nullptr);

    GUI_TYPES::STaskPoint taskPoint;
    taskPoint.taskType = static_cast <GUI_TYPES::TBotTaskType>
            (sender()->property("taskType").toInt());
    const gp_Pnt cursorPos = d_ptr->context->lastCursorPosition();
    taskPoint.globalPos.x = cursorPos.X();
    taskPoint.globalPos.y = cursorPos.Y();
    taskPoint.globalPos.z = cursorPos.Z();

    const gp_Dir normal = d_ptr->context->detectNormal(cursorPos);
    taskPoint.normal.x = normal.X();
    taskPoint.normal.y = normal.Y();
    taskPoint.normal.z = normal.Z();

    CBotTaskDialogFacade dialog(this, taskPoint);
    if (dialog.exec() == QDialog::Accepted)
    {
        d_ptr->context->appendTaskPoint(dialog.getTaskPoint());
        taskPointsChanged();
        d_ptr->viewer->Redraw();
    }
}

void CMainViewport::slChangeTaskPoint()
{
    assert(sender() != nullptr);

    const size_t index = static_cast <size_t> (sender()->property("index").toULongLong());
    if (index < d_ptr->context->getTaskPointCount())
    {
        const GUI_TYPES::STaskPoint taskPoint = d_ptr->context->getTaskPoint(index);
        CBotTaskDialogFacade dialog(this, taskPoint);
        if (dialog.exec() == QDialog::Accepted)
        {
            d_ptr->context->changeTaskPoint(index, dialog.getTaskPoint());
            taskPointsChanged();
            d_ptr->viewer->Redraw();
        }
    }
}

void CMainViewport::slRemoveTaskPoint()
{
    assert(sender() != nullptr);

    const size_t index = static_cast <size_t> (sender()->property("index").toULongLong());
    if (index < d_ptr->context->getTaskPointCount())
    {
        d_ptr->context->removeTaskPoint(index);
        taskPointsChanged();
        d_ptr->viewer->Redraw();
    }
}

void CMainViewport::slAddPathPoint()
{
    const gp_Pnt cursorPos = d_ptr->context->lastCursorPosition();
    GUI_TYPES::SHomePoint initPoint;
    initPoint.globalPos.x = cursorPos.X();
    initPoint.globalPos.y = cursorPos.Y();
    initPoint.globalPos.z = cursorPos.Z();

    const gp_Dir normal = d_ptr->context->detectNormal(cursorPos);
    initPoint.normal.x = normal.X();
    initPoint.normal.y = normal.Y();
    initPoint.normal.z = normal.Z();

    CAddPathPointDialog dialog(this, initPoint);
    if (dialog.exec() == QDialog::Accepted)
    {
        const size_t cnt = d_ptr->context->getHomePointCount();
        for(size_t i = 0; i < cnt; ++i)
            d_ptr->context->removeHomePoint(0);
        d_ptr->context->appendHomePoint(dialog.getHomePoint());
        homePointsChanged();
        d_ptr->viewer->Redraw();
    }
}

void CMainViewport::slChangePathPoint()
{
    assert(sender() != nullptr);

    const size_t index = static_cast <size_t> (sender()->property("index").toULongLong());
    if (index < d_ptr->context->getHomePointCount())
    {
        const GUI_TYPES::SHomePoint pathPoint = d_ptr->context->getHomePoint(index);
        CAddPathPointDialog dialog(this, pathPoint);
        if (dialog.exec() == QDialog::Accepted)
        {
            d_ptr->context->changeHomePoint(index, dialog.getHomePoint());
            homePointsChanged();
            d_ptr->viewer->Redraw();
        }
    }
}

void CMainViewport::slRemovePathPoint()
{
    assert(sender() != nullptr);

    const size_t index = static_cast <size_t> (sender()->property("index").toULongLong());
    if (index < d_ptr->context->getHomePointCount())
    {
        d_ptr->context->removeHomePoint(index);
        homePointsChanged();
        d_ptr->viewer->Redraw();
    }
}

void CMainViewport::savePoints(const QString &fName)
{
    if (!fName.isEmpty())
    {
        CJsonFilePointsSaver saver;
        saver.setFileName(fName.toLatin1());
        const bool res = saver.savePoints(getTaskPoints(),
                                          getHomePoints());
        if (!res)
            QMessageBox::critical(this,
                                  tr("Сохранение задания"),
                                  tr("Не удалось сохранить задание"));
    }
}

CInteractiveContext &CMainViewport::context()
{
    return *d_ptr->context;
}

void CMainViewport::loadPoints(const QString &fName)
{
    if (!fName.isEmpty())
    {
        std::vector <GUI_TYPES::STaskPoint> taskPoints;
        std::vector <GUI_TYPES::SHomePoint> homePoints;
        CJsonFilePointsSaver saver;
        saver.setFileName(fName.toLatin1());
        const bool bRes = saver.loadPoints(taskPoints, homePoints);
        if (!bRes || (taskPoints.empty() && homePoints.empty()))
        {
            QMessageBox::critical(this,
                                  tr("Загрузка задания"),
                                  tr("Не удалось загрузить задание"));
        }
        else
        {
            setTaskPoints(taskPoints);
            setHomePoints(homePoints);
        }
    }
}

void CMainViewport::loadBackupPoints()
{
    loadPoints(backup_points_fname);
}
