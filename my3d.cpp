#include "my3d.h"

#include <QDebug>
#include <QOpenGLContext>

// ---------- 构造 / 析构 ----------
MyGLWidget::MyGLWidget(QWidget *parent) : QOpenGLWidget(parent)
{
    // QOpenGLWidget 默认不参与键盘焦点，必须显式声明，否则收不到 keyPressEvent
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(false);  // 只在按住左键拖拽时处理鼠标移动

    // 主循环驱动：
    //   ticked      —— 固定步长（默认 1/60 秒）的逻辑更新：模型自转、角色推进、键盘推摄像机
    //   frameStepped—— 每帧一次：只请求重绘（渲染与逻辑解耦）
    connect(&m_game_loop, &GameLoop::ticked, this, [this](std::uint64_t) { onGameTick(); });
    connect(&m_game_loop, &GameLoop::frameStepped, this, [this](float) { onGameFrame(); });

    m_game_loop.init();
    m_game_loop.start();

    qInfo().noquote() << "game loop started: fixed tick =" << m_game_loop.fixedTickSeconds()
                      << "s, frame interval =" << m_game_loop.frameInterval() << "ms";
    qInfo().noquote() << "initial camera view:"
                      << (m_camera_view == CameraView::FPV ? "FPV" : "TPV")
                      << "(按 F5 切换人称)";
}

MyGLWidget::~MyGLWidget()
{
    // 删除 GL 对象必须在上下文有效时进行，所以要 makeCurrent()
    makeCurrent();
    releaseGlResources();
    doneCurrent();
}

// ---------- 摄像机 ----------
void MyGLWidget::setCamera(const QVector3D &eye, const QVector3D &target, const QVector3D &up)
{
    MyCamera &camera = m_character.camera();
    camera.setPosition(eye);
    camera.lookAt(target, up);  // 朝向换算成四元数，不保存 target/up
    logCameraPositionIfMoved();
    update();
}

void MyGLWidget::resetCamera()
{
    m_character.setPosition(QVector3D(0.0f, 0.0f, 0.0f));
    m_character.setCameraOffset(QVector3D(0.0f, 2.0f, 5.0f));  // TPV 轨道偏移复位

    MyCamera &camera = m_character.camera();
    camera.setOrientation(QQuaternion());  // 单位四元数 = 看向 -Z、头顶 +Y
    camera.setPerspective(90.0f, 0.1f, 100.0f);
    m_character.syncCamera();  // 按当前人称重新摆放
    syncCharacterModel();

    m_last_logged_camera_position = QVector3D();  // 让下一帧必定记录一次
    logCameraPositionIfMoved();
    update();
}

// ---------- 人称切换器（FPV / TPV）----------
void MyGLWidget::setCameraView(CameraView view)
{
    if (m_camera_view == view) {
        return;
    }

    m_camera_view = view;
    m_character.setView(view);  // 角色按新模式重新摆放摄像机
    clearCameraInput();         // 避免切换瞬间"按键卡住"

    qInfo().noquote() << "camera view:" << (m_camera_view == CameraView::FPV ? "FPV" : "TPV");
    logCameraPositionIfMoved();
    update();
}

void MyGLWidget::clearCameraInput()
{
    m_input = CameraInput{};
}

// ---------- 主循环：固定步长的逻辑更新 ----------
void MyGLWidget::onGameTick()
{
    const float dt = static_cast<float>(m_game_loop.fixedTickSeconds());

    // 每个模型按自己的自转速度推进（固定步长 → 与帧率无关）
    for (MyModel &model : m_models) {
        model.updateSpin(dt);
    }

    // 两种人称都由角色驱动摄像机：FPV 摆在眼睛处，TPV 按轨道偏移摆
    updateCharacter(dt);
}

// ---------- 主循环：每帧只请求重绘 ----------
void MyGLWidget::onGameFrame()
{
    update();
}

// ---------- 输入 → 角色 → 摄像机（FPV 与 TPV 共用这段逻辑）----------
void MyGLWidget::updateCharacter(float dt)
{
    if (dt <= 0.0f) {
        return;
    }

    m_character.controller().setInput(characterInputFromKeys());
    m_character.update(dt);  // 推进角色位置，并按当前人称重新摆放摄像机
    syncCharacterModel();
    logCameraPositionIfMoved();
}

// 键盘状态 → 角色控制器输入
MyCharacterController::InputState MyGLWidget::characterInputFromKeys() const
{
    MyCharacterController::InputState input;
    input.forward = m_input.forward;
    input.backward = m_input.backward;
    input.left = m_input.left;
    input.right = m_input.right;
    input.sprint = m_input.fast;
    input.jump = m_input.up;  // 暂无物理，先接上意图，等加跳跃再实现
    return input;
}

// 角色占位模型：跟着角色走（立方体中心抬到腰部高度）
void MyGLWidget::syncCharacterModel()
{
    if (!m_character_model.hasMesh()) {
        return;
    }
    m_character_model.setPosition(m_character.position() + QVector3D(0.0f, 0.9f, 0.0f));
}

// 机位日志：位置移动或朝向变化超过阈值才写一行（FPV 只转头不动位置，也能记录到）
void MyGLWidget::logCameraPositionIfMoved()
{
    const MyCamera &camera = m_character.camera();
    const QVector3D position = camera.position();
    const QVector3D forward_dir = camera.forward();

    const bool position_changed = m_last_logged_camera_position.isNull()
                                  || (position - m_last_logged_camera_position).length() >= m_log_move_threshold;
    const bool forward_changed = m_last_logged_camera_forward.isNull()
                                 || QVector3D::dotProduct(forward_dir, m_last_logged_camera_forward) < 0.999f;
    if (!position_changed && !forward_changed) {
        return;
    }

    m_last_logged_camera_position = position;
    m_last_logged_camera_forward = forward_dir;
    qInfo().noquote() << QStringLiteral("camera[%1]: eye(%2, %3, %4) forward(%5, %6, %7)")
                             .arg(m_camera_view == CameraView::FPV ? QStringLiteral("FPV") : QStringLiteral("TPV"))
                             .arg(position.x(), 0, 'f', 2)
                             .arg(position.y(), 0, 'f', 2)
                             .arg(position.z(), 0, 'f', 2)
                             .arg(forward_dir.x(), 0, 'f', 3)
                             .arg(forward_dir.y(), 0, 'f', 3)
                             .arg(forward_dir.z(), 0, 'f', 3);
}

// ---------- 键盘：驱动摄像机 ----------
void MyGLWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_W: case Qt::Key_Up:    m_input.forward = true;  break;
        case Qt::Key_S: case Qt::Key_Down:  m_input.backward = true; break;
        case Qt::Key_A: case Qt::Key_Left:  m_input.left = true;     break;
        case Qt::Key_D: case Qt::Key_Right: m_input.right = true;    break;
        case Qt::Key_Space: case Qt::Key_E: m_input.up = true;       break;
        case Qt::Key_C: case Qt::Key_Q:     m_input.down = true;     break;
        case Qt::Key_Shift:                 m_input.fast = true;     break;
        // 人称切换器。注意不用 Tab —— Qt 会在 QWidget::event() 里把 Tab 吃掉用于焦点切换，收不到 keyPressEvent
        case Qt::Key_F5:                    setCameraView(m_camera_view == CameraView::FPV
                                                              ? CameraView::TPV
                                                              : CameraView::FPV); break;
        case Qt::Key_R:                     resetCamera();           break;
        case Qt::Key_Escape:                window()->close();       break;
        default:
            QOpenGLWidget::keyPressEvent(event);  // 未处理的交还基类
            return;
    }
    event->accept();
}

void MyGLWidget::keyReleaseEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_W: case Qt::Key_Up:    m_input.forward = false;  break;
        case Qt::Key_S: case Qt::Key_Down:  m_input.backward = false; break;
        case Qt::Key_A: case Qt::Key_Left:  m_input.left = false;     break;
        case Qt::Key_D: case Qt::Key_Right: m_input.right = false;    break;
        case Qt::Key_Space: case Qt::Key_E: m_input.up = false;       break;
        case Qt::Key_C: case Qt::Key_Q:     m_input.down = false;     break;
        case Qt::Key_Shift:                 m_input.fast = false;     break;
        default:
            QOpenGLWidget::keyReleaseEvent(event);
            return;
    }
    event->accept();
}

// ---------- 滚轮：TPV 拉近拉远跟随距离 / FPV 缩放视场角 ----------
void MyGLWidget::wheelEvent(QWheelEvent *event)
{
    const float steps = static_cast<float>(event->angleDelta().y()) / 120.0f;  // 一格 = 120
    if (steps == 0.0f) {
        event->accept();
        return;
    }

    MyCamera &camera = m_character.camera();
    if (m_camera_view == CameraView::TPV) {
        m_character.setCameraDistance(m_character.cameraDistance() - steps * m_wheel_step);
    } else {
        // FPV：滚轮当变焦用（20°~110°）
        camera.setPerspective(camera.fovYDegrees() - steps * 2.0f,
                              camera.nearPlane(), camera.farPlane());
    }

    logCameraPositionIfMoved();
    update();
    event->accept();
}

// ---------- 鼠标左键拖拽：FPV 自由视角 / TPV 绕角色环绕 ----------
void MyGLWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_orbiting = true;
        m_last_mouse_pos = event->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QOpenGLWidget::mousePressEvent(event);
}

void MyGLWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_orbiting) {
        QOpenGLWidget::mouseMoveEvent(event);
        return;
    }

    const QPoint current_pos = event->position().toPoint();
    const QPoint delta = current_pos - m_last_mouse_pos;
    m_last_mouse_pos = current_pos;

    const float dx = static_cast<float>(delta.x()) * m_orbit_speed;
    const float dy = static_cast<float>(delta.y()) * m_orbit_speed;

    if (m_camera_view == CameraView::FPV) {
        // 第一人称：向右拖 → 视线右转（yaw 负方向）；向下拖 → 低头（pitch 负方向）
        // 俯仰限位由 MyCamera::yawPitch 内部完成（不保存欧拉角）
        m_character.camera().yawPitch(-dx, -dy);
    } else {
        // 第三人称：向右拖 → 相机绕到左侧；向下拖 → 相机升高（"抓住角色拖"的手感）
        m_character.orbitCamera(-dx, dy);
    }

    logCameraPositionIfMoved();
    update();
    event->accept();
}

void MyGLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_orbiting) {
        m_orbiting = false;
        unsetCursor();
        event->accept();
        return;
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

// ---------- 着色器：源码在 shaders/basic.vert / basic.frag，经 qtds.qrc 嵌进 exe ----------
// 编译、链接、错误日志、uniform 位置缓存都由 MyShaderProgram 负责（见 myShader.h/.cpp）
void MyGLWidget::createShaderProgram()
{
    const bool ok = m_program.createFromFiles(
        QStringLiteral(":/shaders/basic.vert"),
        QStringLiteral(":/shaders/basic.frag"),
        // 显式属性号绑定（属性号常量定义在 MyMesh 里，保证 shader 与网格用同一套编号）
        {{MyMesh::kAttribPos, "aPos"}, {MyMesh::kAttribColor, "aColor"}});

    if (!ok) {
        qCritical("着色器程序创建失败，模型将无法绘制");
    }
}

// ---------- 造场景：一份网格 + 多个独立模型 ----------
void MyGLWidget::createScene()
{
    // ① 几何：立方体只上传一次，放进网格库
    std::vector<MyVertex> vertices;
    std::vector<GLuint> indices;
    MyMeshFactory::makeCube(vertices, indices, 1.0f);

    auto cube_mesh = std::make_shared<MyMesh>();
    cube_mesh->create(vertices, indices);
    if (!cube_mesh->isValid()) {
        qCritical("立方体网格创建失败");
        return;
    }
    m_meshes.push_back(cube_mesh);

    // ② 模型：三个独立模型共享同一份几何，但位置、缩放、自转各不相同
    struct ModelSpec
    {
        QVector3D position;
        float scale;
        float spin_degrees_per_second;
        QVector3D spin_axis;
    };

    const ModelSpec specs[] = {
        {QVector3D(-4.0f, 0.0f, -2.0f), 1.0f, 30.0f, QVector3D(0.0f, 1.0f, 0.0f)},
        {QVector3D(4.0f, 0.0f, -2.0f), 1.4f, 60.0f, QVector3D(1.0f, 0.0f, 0.0f)},
        {QVector3D(0.0f, 0.0f, -7.0f), 0.7f, 90.0f, QVector3D(1.0f, 1.0f, 0.0f)},
    };

    for (const ModelSpec &spec : specs) {
        MyModel model(cube_mesh);  // ★ 共享同一份网格
        model.setPosition(spec.position);
        model.setUniformScale(spec.scale);
        model.setSpin(spec.spin_degrees_per_second, spec.spin_axis);
        m_models.push_back(std::move(model));
    }

    // ③ 角色占位模型：复用同一份立方体几何，缩小成"人形占位"，位置每帧跟随角色
    m_character_model = MyModel(cube_mesh);
    m_character_model.setUniformScale(0.6f);
    m_character_model.setSpin(0.0f);  // 角色不转
    syncCharacterModel();

    qInfo().noquote() << "scene:" << m_meshes.size() << "mesh(es),"
                      << (m_models.size() + 1) << "model(s)";
}

// ---------- 显式释放所有 GL 资源（需要当前上下文） ----------
void MyGLWidget::releaseGlResources()
{
    // 网格对象析构时会自己 glDelete*，这里清空容器即可（shared_ptr 引用计数归零）
    m_models.clear();
    m_meshes.clear();

    m_program.destroy();
}

// ---------- 初始化 ----------
void MyGLWidget::initializeGL()
{
    if (!initializeOpenGLFunctions()) {
        qCritical("无法加载 OpenGL 4.3 Core 函数（上下文版本过低，请检查 main.cpp 的 QSurfaceFormat）");
        return;
    }

    const QSurfaceFormat fmt = context()->format();
    qInfo().noquote() << "GL context:" << fmt.majorVersion() << "." << fmt.minorVersion()
                      << "| core profile:" << (fmt.profile() == QSurfaceFormat::CoreProfile)
                      << "| GL_VERSION:" << reinterpret_cast<const char *>(glGetString(GL_VERSION))
                      << "| GPU:" << reinterpret_cast<const char *>(glGetString(GL_RENDERER));

    glClearColor(0.1f, 0.12f, 0.15f, 0.1f);
    glEnable(GL_DEPTH_TEST);
    // glEnable(GL_MULTISAMPLE);   // 按你的改动保持关闭

    createShaderProgram();
    createScene();

    // 初始化阶段显式检查一次 GL 错误
    const GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        qWarning() << "初始化后存在 GL 错误, code = 0x" << Qt::hex << err;
    }
}

// ---------- 尺寸变化 ----------
void MyGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    // 只把新的纵横比告诉摄像机；fov / near / far 由摄像机自己持有
    m_character.camera().setViewportAspect(h > 0 ? float(w) / float(h) : 1.0f);
}

// ---------- 绘制：遍历模型列表，一个模型一次 draw call ----------
void MyGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const MyCamera &camera = m_character.camera();
    const QMatrix4x4 view_projection = camera.projectionMatrix() * camera.viewMatrix();

    m_program.bind();  // 所有模型共用同一份着色器程序

    // 场景模型（自转的立方体）
    for (const MyModel &model : m_models) {
        MyMesh *mesh = model.mesh();
        if (mesh == nullptr || !mesh->isValid()) {
            continue;
        }

        // 每个模型有自己的模型矩阵 → 每个模型重新写一次 uniform（位置由 MyShaderProgram 缓存）
        m_program.setMat4("uMvp", view_projection * model.modelMatrix());
        mesh->draw();
    }

    // 角色占位模型
    if (MyMesh *mesh = m_character_model.mesh(); mesh != nullptr && mesh->isValid()) {
        m_program.setMat4("uMvp", view_projection * m_character_model.modelMatrix());
        mesh->draw();
    }

    m_program.release();
}
