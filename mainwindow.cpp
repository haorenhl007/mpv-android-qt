#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "androidfiledialog.h"

#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>
#include <jni.h>

#include <qpa/qplatformnativeinterface.h>

#include <android/native_window.h>
#include <android/native_window_jni.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    QString mpvConfigDir = configDir();
    QDir().mkpath(mpvConfigDir);
    QStringList fileNeedCopy;

    fileNeedCopy << "cacert.pem" << "subfont.ttf" << "stats.lua";
    foreach(QString fileName, fileNeedCopy) {
        QFile file("assets:/" + fileName);
        if (!QFileInfo(mpvConfigDir + fileName).isFile()) {
            file.copy(QString(mpvConfigDir + fileName));
        }
    }

    ui->setupUi(this);

    m_mpv = ui->openGLWidget;

    connect(ui->videoSlider, SIGNAL(sliderMoved(int)), SLOT(seek(int)));
    connect(ui->pushButton, SIGNAL(clicked()), SLOT(openMedia()));
    connect(ui->pushButton2, SIGNAL(clicked()), SLOT(pauseResume()));
    connect(ui->pushButton3, SIGNAL(clicked()), SLOT(switchStatScript()));
    connect(m_mpv, SIGNAL(positionChanged(int)), ui->videoSlider, SLOT(setValue(int)));
    connect(m_mpv, SIGNAL(durationChanged(int)), this, SLOT(setSliderRange(int)));

    //Rotation
    QScreen *s = QGuiApplication::primaryScreen();
    connect(s, SIGNAL(orientationChanged(Qt::ScreenOrientation)),
        this, SLOT(orientationChanged(Qt::ScreenOrientation)));

    s->setOrientationUpdateMask(
        Qt::PortraitOrientation
        | Qt::LandscapeOrientation
        | Qt::InvertedPortraitOrientation
        | Qt::InvertedLandscapeOrientation);

    QTimer::singleShot(50, this, SLOT(checkIntentContent()));

    m_mpv->command(QVariantList() << "load-script" << configDir() + "stats.lua");
}

MainWindow::~MainWindow()
{
    delete ui;
}

QString MainWindow::configDir()
{
    return "/storage/emulated/0/mpv/";
}

void MainWindow::checkIntentContent()
{
    //handle intent
    QString fileFromIntent = QAndroidJniObject::callStaticObjectMethod("com.harry.mpv.QtFullscreenActivity"
                                                                                  , "getUrl"
                                                                                  , "()Ljava/lang/String;")
                .toString();

    qDebug() << "open file:" << fileFromIntent;
    if (!fileFromIntent.isEmpty()) {
        openFileNameReady(fileFromIntent);
    }

    //init surface
    QPlatformNativeInterface *nativeInterfaceAPP = QApplication::platformNativeInterface();
    jobject activity = (jobject)(nativeInterfaceAPP->nativeResourceForIntegration("QtActivity"));
    QAndroidJniEnvironment * qjniEnv;
    JNIEnv * jniEnv;
    JavaVM * jvm = qjniEnv->javaVM();
    jvm->GetEnv(reinterpret_cast<void**>(&qjniEnv), JNI_VERSION_1_6);
    jvm->AttachCurrentThread(&jniEnv,NULL);

    jint r_id_content = QAndroidJniObject::getStaticField<jint>("android/R$id", "content");

    QAndroidJniObject view = ((QAndroidJniObject) activity).callObjectMethod("findViewById", "(I)Landroid/view/View;", r_id_content);
    if (view.isValid()) {
        QAndroidJniObject child1 = view.callObjectMethod("getChildAt", "(I)Landroid/view/View;", 0);
        QAndroidJniObject child2 = child1.callObjectMethod("getChildAt", "(I)Landroid/view/View;", 0);
        if (child2.isValid()) {
            QAndroidJniObject sHolder = child2.callObjectMethod("getHolder","()Landroid/view/SurfaceHolder;");
            if (sHolder.isValid()) {
                QAndroidJniObject theSurface = sHolder.callObjectMethod("getSurface","()Landroid/view/Surface;");
                if (theSurface.isValid()) {
                    ANativeWindow* awindow = ANativeWindow_fromSurface(jniEnv, theSurface.object());
                    qDebug() << "This is a ANativeWindow " << awindow;
                    WId wid = winId();
                    qDebug() << "winid returned " << wid;
                }
            }
        } else {
            qDebug() << "Views are not loaded yet or you are not in the Qt UI Thread";
        }
    }

}

void MainWindow::openMedia()
{
    //QString file = QFileDialog::getOpenFileName(0, "Open a video");
    //if (file.isEmpty())
    //    return;

    AndroidFileDialog *fileDialog = new AndroidFileDialog();
    connect(fileDialog, SIGNAL(existingFileNameReady(QString)), this, SLOT(openFileNameReady(QString)));
    bool success = fileDialog->provideExistingFileName();
    if (!success) {
        qDebug() << "Problem with JNI or sth like that...";
        disconnect(fileDialog, SIGNAL(existingFileNameReady(QString)), this, SLOT(openFileNameReady(QString)));
        //or just delete fileDialog instead of disconnect
    }
}

void MainWindow::openFileNameReady(QString fileName)
{
    if (!fileName.isNull()) {
        qDebug() << "FileName: " << fileName;
        m_mpv->command(QStringList() << "loadfile" << fileName);
    } else {
        qDebug() << "User did not choose file";
    }    
}

void MainWindow::seek(int pos)
{
    m_mpv->command(QVariantList() << "seek" << pos << "absolute");
}

void MainWindow::switchSub()
{
    m_mpv->command(QVariantList() << "cycle" << "sub");   
}

void MainWindow::pauseResume()
{
    const bool paused = m_mpv->getProperty("pause").toBool();
    m_mpv->setProperty("pause", !paused);
}

void MainWindow::setSliderRange(int duration)
{
    ui->videoSlider->setRange(0, duration);
}

void MainWindow::orientationChanged(Qt::ScreenOrientation orientation)
{
    qDebug("orientationChanged");
    if(orientation == Qt::LandscapeOrientation || orientation == Qt::InvertedLandscapeOrientation)
    {
        ui->videoSlider->setVisible(false);
        ui->pushButton->setVisible(false);
        ui->pushButton2->setVisible(false);
        ui->pushButton3->setVisible(false);
        ui->horizontalLayout->setSpacing(0);
    }
    else
    {
        ui->videoSlider->setVisible(true);
        ui->pushButton->setVisible(true);
        ui->pushButton2->setVisible(true);
        ui->pushButton3->setVisible(true);
        ui->horizontalLayout->setSpacing(7);
    }
}

void MainWindow::changeOrientation()
{
    QAndroidJniEnvironment env;
    QAndroidJniObject activity = QtAndroid::androidActivity();
    jint orient = activity.callMethod<jint>( "getRequestedOrientation" );
    if(env->ExceptionCheck())
    {
        qDebug() << "exception occured";
        env->ExceptionClear();
    }

    orient = !orient;
    activity.callMethod<void>("setRequestedOrientation", "(I)V", orient);
    if(env->ExceptionCheck())
    {
        qDebug() << "exception occured";
        env->ExceptionClear();
    }
}

void MainWindow::switchStatScript()
{
    m_mpv->command(QVariantList() << "script-message" << "togglestats");
    //m_mpv->command(QVariantList() << "load-script" << configDir() + "stats.lua");
}
