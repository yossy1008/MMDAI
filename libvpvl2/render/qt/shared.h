/* ----------------------------------------------------------------- */
/*                                                                   */
/*  Copyright (c) 2009-2011  Nagoya Institute of Technology          */
/*                           Department of Computer Science          */
/*                2010-2012  hkrn                                    */
/*                                                                   */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/* - Redistributions of source code must retain the above copyright  */
/*   notice, this list of conditions and the following disclaimer.   */
/* - Redistributions in binary form must reproduce the above         */
/*   copyright notice, this list of conditions and the following     */
/*   disclaimer in the documentation and/or other materials provided */
/*   with the distribution.                                          */
/* - Neither the name of the MMDAI project team nor the names of     */
/*   its contributors may be used to endorse or promote products     */
/*   derived from this software without specific prior written       */
/*   permission.                                                     */
/*                                                                   */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND            */
/* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,       */
/* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF          */
/* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE          */
/* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS */
/* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,          */
/* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED   */
/* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON */
/* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,   */
/* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    */
/* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE           */
/* POSSIBILITY OF SUCH DAMAGE.                                       */
/* ----------------------------------------------------------------- */

#include <vpvl2/vpvl2.h>
#include <vpvl2/IRenderDelegate.h>

#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <QtOpenGL/QtOpenGL>

#ifndef VPVL2_NO_BULLET
#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>
#else
VPVL2_DECLARE_HANDLE(btDiscreteDynamicsWorld)
#endif

#ifdef VPVL2_LINK_ASSIMP
#include <assimp.hpp>
#include <DefaultLogger.h>
#include <Logger.h>
#include <aiPostProcess.h>
#else
BT_DECLARE_HANDLE(aiScene);
#endif

/* internal headers */
#include "vpvl2/pmx/Bone.h"
#include "vpvl2/pmx/Joint.h"
#include "vpvl2/pmx/Label.h"
#include "vpvl2/pmx/Material.h"
#include "vpvl2/pmx/Model.h"
#include "vpvl2/pmx/Morph.h"
#include "vpvl2/pmx/RigidBody.h"
#include "vpvl2/pmx/Vertex.h"

#include "vpvl2/vmd/Motion.h"

using namespace vpvl2;

namespace
{
static const int kWidth = 800;
static const int kHeight = 600;
static const int kFPS = 60;

static const QString kSystemTexturesDir = "../../QMA2/resources/images";
static const QString kShaderProgramsDir = "../../QMA2/resources/shaders";
static const QString kKernelProgramsDir = "../../QMA2/resources/kernels";
static const QString kModelDir = "render/res/miku2";
static const QString kStageDir = "render/res/stage";
static const QString kMotion = "render/res/motion.vmd";
static const QString kCamera = "render/res/camera.vmd";
static const QString kModelName = "miku.pmx";
static const QString kStageName = "stage.x";
static const QString kStage2Name = "stage2.x";

static const qreal kCameraNear = 0.5;
static const qreal kCameraFar = 10000.0;

typedef QScopedPointer<uint8_t, QScopedPointerArrayDeleter<uint8_t> > ByteArrayPtr;

static bool UISlurpFile(const QString &path, QByteArray &bytes) {
    QFile file(path);
    if (file.open(QFile::ReadOnly)) {
        bytes = file.readAll();
        file.close();
        return true;
    }
    else {
        qWarning("slurpFile error at %s: %s", qPrintable(path), qPrintable(file.errorString()));
        return false;
    }
}

class String : public IString {
public:
    String(const QString &s)
        : m_bytes(s.toUtf8()),
          m_value(s)
    {
    }
    ~String() {
    }

    IString *clone() const {
        return new String(m_value);
    }
    const HashString toHashString() const {
        return HashString(m_bytes.constData());
    }
    bool equals(const IString *value) const {
        return m_value == static_cast<const String *>(value)->value();
    }
    const QString &value() const {
        return m_value;
    }
    const uint8_t *toByteArray() const {
        return reinterpret_cast<const uint8_t *>(m_bytes.constData());
    }
    size_t length() const {
        return m_bytes.length();
    }

private:
    QByteArray m_bytes;
    QString m_value;
};

class Encoding : public IEncoding {
public:
    Encoding()
        : m_sjis(QTextCodec::codecForName("Shift-JIS")),
          m_utf8(QTextCodec::codecForName("UTF-8")),
          m_utf16(QTextCodec::codecForName("UTF-16"))
    {
    }
    ~Encoding() {
    }

    IString *toString(const uint8_t *value, size_t size, IString::Codec codec) const {
        IString *s = 0;
        const char *str = reinterpret_cast<const char *>(value);
        switch (codec) {
        case IString::kShiftJIS:
            s = new String(m_sjis->toUnicode(str, size));
            break;
        case IString::kUTF8:
            s = new String(m_utf8->toUnicode(str, size));
            break;
        case IString::kUTF16:
            s = new String(m_utf16->toUnicode(str, size));
            break;
        }
        return s;
    }
    IString *toString(const uint8_t *value, IString::Codec codec, size_t maxlen) const {
        size_t size = qstrnlen(reinterpret_cast<const char *>(value), maxlen);
        return toString(value, size, codec);
    }
    uint8_t *toByteArray(const IString *value, IString::Codec codec) const {
        const String *s = static_cast<const String *>(value);
        QByteArray bytes;
        switch (codec) {
        case IString::kShiftJIS:
            bytes = m_sjis->fromUnicode(s->value());
            break;
        case IString::kUTF8:
            bytes = m_utf8->fromUnicode(s->value());
            break;
        case IString::kUTF16:
            bytes = m_utf16->fromUnicode(s->value());
            break;
        }
        size_t size = bytes.length();
        uint8_t *data = new uint8_t[size + 1];
        memcpy(data, bytes.constData(), size);
        data[size] = 0;
        return data;
    }
    void disposeByteArray(uint8_t *value) const {
        delete[] value;
    }

private:
    QTextCodec *m_sjis;
    QTextCodec *m_utf8;
    QTextCodec *m_utf16;
};

class Delegate : public IRenderDelegate
{
public:
    struct PrivateContext {
        QHash<QString, GLuint> textureCache;
    };
    Delegate(QGLWidget *widget)
        : m_widget(widget)
    {
    }
    ~Delegate()
    {
    }

    void allocateContext(const IModel *model, void *&context) {
        const IString *name = model->name();
        PrivateContext *ctx = new(std::nothrow) PrivateContext();
        context = ctx;
        qDebug("Allocated the context: %s", name ? name->toByteArray() : reinterpret_cast<const uint8_t *>("(null)"));
    }
    void releaseContext(const IModel *model, void *&context) {
        const IString *name = model->name();
        delete static_cast<PrivateContext *>(context);
        context = 0;
        qDebug("Released the context: %s", name ? name->toByteArray() : reinterpret_cast<const uint8_t *>("(null)"));
    }
    bool uploadTexture(void *context, const IString *name, const IString *dir, void *texture, bool isToon) {
        return uploadTextureInternal(createPath(dir, name), texture, isToon, context);
    }
    bool uploadToonTexture(void *context, const char *name, const IString *dir, void *texture) {
        if (!uploadTextureInternal(createPath(dir, name), texture, true, context)) {
            String s(kSystemTexturesDir);
            return uploadTextureInternal(createPath(&s, name), texture, true, context);
        }
        return true;
    }
    bool uploadToonTexture(void *context, const IString *name, const IString *dir, void *texture) {
        if (!uploadTextureInternal(createPath(dir, name), texture, true, context)) {
            String s(kSystemTexturesDir);
            return uploadTextureInternal(createPath(&s, name), texture, true, context);
        }
        return true;
    }
    bool uploadToonTexture(void *context, int index, void *texture) {
        QString format;
        QDir dir(kSystemTexturesDir);
        const QString &pathString = dir.absoluteFilePath(format.sprintf("toon%02d.bmp", index + 1));
        return uploadTextureInternal(pathString, texture, true, context);
    }

    void log(void * /* context */, LogLevel /* level */, const char *format, va_list ap) {
        vfprintf(stderr, format, ap);
        fprintf(stderr, "%s", "\n");
    }
    IString *loadKernelSource(KernelType type, void * /* context */) {
        QString file;
        switch (type) {
        case kModelSkinningKernel:
            file = "skinning.cl";
            break;
        }
        QByteArray bytes;
        QString path = kKernelProgramsDir + "/" + file;
        if (UISlurpFile(path, bytes)) {
            qDebug("Loaded a kernel: %s", qPrintable(path));
            return new(std::nothrow) String(bytes);
        }
        else {
            return 0;
        }
    }
    IString *loadShaderSource(ShaderType type, const IModel *model, void * /* context */) {
        QString file;
        switch (model->type()) {
        case IModel::kAsset:
            file += "asset/";
            break;
        case IModel::kPMD:
            file += "pmd/";
            break;
        case IModel::kPMX:
            file += "pmx/";
            break;
        }
        switch (type) {
        case kEdgeVertexShader:
            file += "edge.vsh";
            break;
        case kEdgeFragmentShader:
            file += "edge.fsh";
            break;
        case kModelVertexShader:
            file += "model.vsh";
            break;
        case kModelFragmentShader:
            file += "model.fsh";
            break;
        case kShadowVertexShader:
            file += "shadow.vsh";
            break;
        case kShadowFragmentShader:
            file += "shadow.fsh";
            break;
        case kZPlotVertexShader:
            file += "zplot.vsh";
            break;
        case kZPlotFragmentShader:
            file += "zplot.fsh";
            break;
        }
        QByteArray bytes;
        QString path = kShaderProgramsDir + "/" + file;
        if (UISlurpFile(path, bytes)) {
            qDebug("Loaded a shader: %s", qPrintable(path));
            return new(std::nothrow) String(bytes);
        }
        else {
            return 0;
        }
    }
    IString *toUnicode(const uint8_t *value) const {
        QTextCodec *codec = QTextCodec::codecForName("Shift-JIS");
        const QString &s = codec->toUnicode(reinterpret_cast<const char *>(value));
        return new(std::nothrow) String(s);
    }

private:
    static const QString createPath(const IString *dir, const char *name) {
        return createPath(dir, QString(name));
    }
    static const QString createPath(const IString *dir, const QString &name) {
        const QDir d(static_cast<const String *>(dir)->value());
        return d.absoluteFilePath(name);
    }
    static const QString createPath(const IString *dir, const IString *name) {
        const QDir d(static_cast<const String *>(dir)->value());
        return d.absoluteFilePath(static_cast<const String *>(name)->value());
    }
    static void setTextureID(GLuint textureID, bool isToon, void *texture) {
        *static_cast<GLuint *>(texture) = textureID;
        if (!isToon) {
            glTexParameteri(textureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(textureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }
    bool uploadTextureInternal(const QString &path, void *texture, bool isToon, void *context) {
        const QFileInfo info(path);
        if (info.isDir() || !info.exists()) {
            qWarning("Cannot loading \"%s\"", qPrintable(path));
            return false;
        }
        PrivateContext *ctx = static_cast<PrivateContext *>(context);
        if (ctx->textureCache.contains(path)) {
            setTextureID(ctx->textureCache[path], isToon, texture);
            return true;
        }
        const QImage &image = QImage(path).rgbSwapped();
        QGLContext::BindOptions options = QGLContext::LinearFilteringBindOption|QGLContext::InvertedYBindOption;
        GLuint textureID = m_widget->bindTexture(QGLWidget::convertToGLFormat(image), GL_TEXTURE_2D, GL_RGBA, options);
        setTextureID(textureID, isToon, texture);
        ctx->textureCache.insert(path, textureID);
        qDebug("Loaded a texture (ID=%d): \"%s\"", textureID, qPrintable(path));
        return textureID != 0;
    }

    QGLWidget *m_widget;
};
} /* namespace anonymous */

QDebug operator<<(QDebug debug, const Vector3 &v)
{
    debug.nospace() << "(x=" << v.x() << ", y=" << v.y() << ", z=" << v.z() << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const Color &v)
{
    debug.nospace() << "(r=" << v.x() << ", g=" << v.y() << ", b=" << v.z() << ", a=" << v.w() << ")";
    return debug;
}

QDebug operator<<(QDebug debug, const IString *str)
{
    if (str) {
        debug.nospace() << reinterpret_cast<const String *>(str)->value();
    }
    else {
        debug.nospace() << "\"\"";
    }
    return debug;
}

QDebug operator<<(QDebug debug, const pmx::Bone *bone)
{
    debug.nospace()
            << "Bone id=" << bone->index()
            << " name=" << bone->name()
            << " english=" << bone->englishName()
            << " origin=" << bone->origin();
    if (bone->parentBone())
        debug << " parent=" << bone->parentBone()->name();
    debug << " index=" << bone->layerIndex() << "\n" << " offset=" << bone->origin();
    if (bone->isIKEnabled()) {
        debug << " targetBone=" << bone->targetBone()->name()
              << " constraintAngle=" << bone->constraintAngle();
    }
    if (bone->hasPositionInherence()) {
        debug << " parentPositionInherenceBone=" << bone->parentInherenceBone()->name()
              << " weight=" << bone->weight();
    }
    if (bone->hasRotationInherence()) {
        debug << " parentRotationInherenceBone=" << bone->parentInherenceBone()->name()
              << " weight=" << bone->weight();
    }
    if (bone->isAxisFixed())
        debug << " axis=" << bone->axis();
    if (bone->hasLocalAxis())
        debug << " axisX=" << bone->axisX() << " axisZ=" << bone->axisZ();
    return debug.space();
}

QDebug operator<<(QDebug debug, const pmx::Material *material)
{
    debug.nospace()
            << "Material name=" << material->name()
            << " english=" << material->englishName()
            << " mainTexture=" << material->mainTexture()
            << " sphereTexture=" << material->sphereTexture()
            << " toonTexture=" << material->toonTexture()
            << "\n"
            << " ambient=" << material->ambient()
            << " diffuse=" << material->diffuse()
            << " specular=" << material->specular()
            << " edgeColor=" << material->edgeColor()
            << "\n"
            << " shininess=" << material->shininess()
            << " edgeSize=" << material->edgeSize()
            << " indices=" << material->indices()
            << " isSharedToonTextureUsed=" << material->isSharedToonTextureUsed()
            << " isCullDisabled=" << material->isCullFaceDisabled()
            << " hasShadow=" << material->hasShadow()
            << " isShadowMapDrawin=" << material->isShadowMapDrawn()
            << " isEdgeDrawn=" << material->isEdgeDrawn()
               ;
    switch (material->sphereTextureRenderMode()) {
    case pmx::Material::kAddTexture:
        qDebug() << "sphere=add";
        break;
    case pmx::Material::kMultTexture:
        qDebug() << "sphere=modulate";
        break;
    case pmx::Material::kNone:
        qDebug() << "sphere=none";
        break;
    case pmx::Material::kSubTexture:
        qDebug() << "sphere=sub";
        break;
    }
    return debug.space();
}

QDebug operator<<(QDebug debug, const pmx::Morph *morph)
{
    debug.nospace()
            << "Morph name=" << morph->name()
            << " english=" << morph->englishName()
               ;
    return debug.space();
}

class UI : public QGLWidget
{
public:
    UI()
        : QGLWidget(QGLFormat(QGL::SampleBuffers), 0),
      #ifndef VPVL2_NO_BULLET
          m_dispatcher(&m_config),
          m_broadphase(Vector3(-10000.0f, -10000.0f, -10000.0f), Vector3(10000.0f, 10000.0f, 10000.0f), 1024),
          m_world(&m_dispatcher, &m_broadphase, &m_solver, &m_config),
      #endif /* VPVL2_NO_BULLET */
          m_delegate(this),
          m_factory(0),
          m_encoding(0),
          m_prevElapsed(0),
          m_currentFrameIndex(0)
    {
        Encoding *encoding = new Encoding();
        m_encoding = encoding;
        m_factory = new Factory(encoding);
#ifndef VPVL2_NO_BULLET
        m_world.setGravity(btVector3(0.0f, -9.8f * 2.0f, 0.0f));
        m_world.getSolverInfo().m_numIterations = static_cast<int>(10.0f);
#endif /* VPVL2_NO_BULLET */
    }
    ~UI() {
#ifdef VPVL2_LINK_ASSIMP
        Assimp::DefaultLogger::kill();
#endif
        delete m_factory;
        delete m_encoding;
    }

    void rotate(float x, float y) {
        Scene::ICamera *camera = m_scene.camera();
        Vector3 angle = camera->angle();
        angle.setX(angle.x() + x);
        angle.setY(angle.y() + y);
        camera->setAngle(angle);
    }
    void translate(float x, float y) {
        Scene::ICamera *camera = m_scene.camera();
        Vector3 position = camera->position();
        position.setX(position.x() + x);
        position.setY(position.y() + y);
        camera->setPosition(position);
    }

protected:
    void initializeGL() {
        if (!loadScene())
            qFatal("Unable to load scene");

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        resize(kWidth, kHeight);
        startTimer(1000.0f / 60.0f);
        m_timer.start();
    }
    void timerEvent(QTimerEvent *) {
        float elapsed = m_timer.elapsed() / static_cast<float>(60.0f);
        float diff = elapsed - m_prevElapsed;
        m_prevElapsed = elapsed;
        if (diff < 0)
            diff = elapsed;
        const Array<IRenderEngine *> &engines = m_scene.renderEngines();
        const int nengines = engines.count();
        for (int i = 0; i < nengines; i++)
            engines[i]->update();
        updateGL();
    }
    void mousePressEvent(QMouseEvent *event) {
        m_prevPos = event->pos();
    }
    void mouseMoveEvent(QMouseEvent *event) {
        if (event->buttons() & Qt::LeftButton) {
            Qt::KeyboardModifiers modifiers = event->modifiers();
            const QPoint &diff = event->pos() - m_prevPos;
            if (modifiers & Qt::ShiftModifier) {
                translate(diff.x() * -0.1f, diff.y() * 0.1f);
            }
            else {
                rotate(diff.y() * 0.5f, diff.x() * 0.5f);
            }
            m_prevPos = event->pos();
        }
    }
    void keyPressEvent(QKeyEvent *event) {
        const Array<IMotion *> &motions = m_scene.motions();
        const int nmotions = motions.count();
        if (nmotions > 0 && event->modifiers() & Qt::SHIFT) {
            switch (event->key()) {
            case Qt::Key_Left:
                m_currentFrameIndex -= 1.0f;
                btSetMax(m_currentFrameIndex, 0.0f);
                for (int i = 0; i < nmotions; i++)
                    motions[i]->seek(m_currentFrameIndex);
                break;
            case Qt::Key_Right:
                m_currentFrameIndex += 1.0;
                for (int i = 0; i < nmotions; i++) {
                    IMotion *motion = motions[i];
                    btSetMin(m_currentFrameIndex, motion->maxFrameIndex());
                    motion->seek(m_currentFrameIndex);
                }
                break;
            }
            m_scene.seek(m_currentFrameIndex);
            qDebug() << m_currentFrameIndex;
            for (int i = 0; i < nmotions; i++) {
                IMotion *motion = motions[i];
                if (motion->isReachedTo(motion->maxFrameIndex()))
                    motion->reset();
            }
            const int kFPS = 30;
            const Scalar &sec = 1.0 / kFPS;
            m_world.stepSimulation(sec, 1, 1.0 / kFPS);
        }
    }
    void wheelEvent(QWheelEvent *event) {
        Qt::KeyboardModifiers modifiers = event->modifiers();
        Scene::ICamera *camera = m_scene.camera();
        if (modifiers & Qt::ControlModifier && modifiers & Qt::ShiftModifier) {
            const qreal step = 1.0;
            camera->setFovy(qMax(event->delta() > 0 ? camera->fovy() - step : camera->fovy() + step, 0.0));
        }
        else {
            qreal step = 4.0;
            if (modifiers & Qt::ControlModifier)
                step *= 5.0f;
            else if (modifiers & Qt::ShiftModifier)
                step *= 0.2f;
            if (step != 0.0f)
                camera->setDistance(event->delta() > 0 ? camera->distance() - step : camera->distance() + step);
        }
    }
    void resizeGL(int w, int h) {
        glViewport(0, 0, w, h);
    }
    void paintGL() {
        glEnable(GL_DEPTH_TEST);
        glClearColor(0, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        updateModelViewMatrix();
        updateProjectionMatrix();
        updateModelViewProjectionMatrix();
        const Array<IRenderEngine *> &engines = m_scene.renderEngines();
        const int nengines = engines.count();
        for (int i = 0; i < nengines; i++) {
            IRenderEngine *engine = engines[i];
            engine->update();
            engine->renderModel();
            engine->renderEdge();
            engine->renderShadow();
        }
    }

private:
    void updateModelViewMatrix() {
        float matrixf[16];
        m_scene.matrices()->getModelView(matrixf);
        for (int i = 0; i < 16; i++)
            m_modelViewMatrix.data()[i] = matrixf[i];
    }
    void updateProjectionMatrix() {
        float matrixf[16];
        m_projectionMatrix.setToIdentity();
        m_projectionMatrix.perspective(m_scene.camera()->fovy(), kWidth / float(kHeight), kCameraNear, kCameraFar);
        for (int i = 0; i < 16; i++)
            matrixf[i] = m_projectionMatrix.constData()[i];
        m_scene.matrices()->setProjection(matrixf);
    }
    void updateModelViewProjectionMatrix() {
        float matrixf[16];
        const QMatrix4x4 &result = m_projectionMatrix * m_modelViewMatrix;
        for (int i = 0; i < 16; i++)
            matrixf[i] = result.constData()[i];
        m_scene.matrices()->setModelViewProjection(matrixf);
    }
    bool loadScene() {
#ifdef VPVL2_LINK_ASSIMP
        Assimp::Logger::LogSeverity severity = Assimp::Logger::VERBOSE;
        Assimp::DefaultLogger::create("", severity, aiDefaultLogStream_STDOUT);
        addModel(QDir(kStageDir).absoluteFilePath(kStageName));
        // addModel(kStage2Name, kStageDir);
#endif
        addMotion(kMotion, addModel(QDir(kModelDir).absoluteFilePath(kModelName)));
        // addMotion(kMotion, addModel(QString("miku.pmx"), "render/res/lat"));
        QByteArray bytes;
        if (UISlurpFile(kCamera, bytes)) {
            bool ok = true;
            const uint8_t *data = reinterpret_cast<const uint8_t *>(bytes.constData());
            IMotion *motion = m_factory->createMotion(data, bytes.size(), 0, ok);
            qDebug() << "maxFrameIndex(camera):" << motion->maxFrameIndex();
            m_scene.camera()->setMotion(motion);
        }
        m_scene.seek(0);
        return true;
    }
    IModel *addModel(const QString &path) {
        QByteArray bytes;
        if (!UISlurpFile(path, bytes)) {
            qWarning("Failed loading the model");
            return 0;
        }
        return addModel(bytes, QFileInfo(path).absoluteDir().path());
    }
    IModel *addModel(const QByteArray &bytes, const QString &dir) {
        bool ok = true;
        const uint8_t *data = reinterpret_cast<const uint8_t *>(bytes.constData());
        IModel *model = m_factory->createModel(data, bytes.size(), ok);
        if (!ok) {
            qWarning("Failed parsing the model: %d", model->error());
            return 0;
        }
        model->joinWorld(&m_world);
        IRenderEngine *engine = m_scene.createRenderEngine(&m_delegate, model);
        String s(dir);
        engine->upload(&s);
        m_scene.addModel(model, engine);
#if 0
        pmx::Model *model = static_cast<pmx::Model*>(m_model);
        for (int i = 0; i < model->materials().count(); i++)
            qDebug() << model->materials().at(i);
        for (int i = 0; i < model->bones().count(); i++)
            qDebug() << model->bones().at(i);
        for (int i = 0; i < model->morphs().count(); i++)
            qDebug() << model->morphs().at(i);
        for (int i = 0; i < m_model->rigidBodies().count(); i++)
            qDebug("rbody%d: %s", i, m_delegate.toUnicode(m_model->rigidBodies()[i]->name()).c_str());
        for (int i = 0; i < m_model->joints().count(); i++)
            qDebug("joint%d: %s", i, m_delegate.toUnicode(m_model->joints()[i]->name()).c_str());
#endif
        return model;
    }
    void addMotion(const QString &path, IModel *model) {
        QByteArray bytes;
        if (model && UISlurpFile(path, bytes)) {
            bool ok = true;
            IMotion *motion = m_factory->createMotion(reinterpret_cast<const uint8_t *>(bytes.constData()), bytes.size(), model, ok);
            qDebug() << "maxFrameIndex(model):" << motion->maxFrameIndex();
            motion->seek(0);
            m_scene.addMotion(motion);
        }
        else {
            qWarning("Failed parsing the model motion, skipped...");
        }
    }

#ifndef VPVL2_NO_BULLET
    btDefaultCollisionConfiguration m_config;
    btCollisionDispatcher m_dispatcher;
    btAxisSweep3 m_broadphase;
    btSequentialImpulseConstraintSolver m_solver;
    btDiscreteDynamicsWorld m_world;
#endif /* VPVL2_NO_BULLET */
    QElapsedTimer m_timer;
    QPoint m_prevPos;
    QMatrix4x4 m_projectionMatrix;
    QMatrix4x4 m_modelViewMatrix;
    Delegate m_delegate;
    Scene m_scene;
    Factory *m_factory;
    IEncoding *m_encoding;
    float m_prevElapsed;
    float m_currentFrameIndex;
};

