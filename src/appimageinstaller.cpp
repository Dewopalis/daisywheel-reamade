#include "appimageinstaller.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>

namespace {
QString installPath() {
    return QDir::homePath() + "/.local/opt/daisywheel-reamade/daisywheel-reamade.AppImage";
}

bool installImage(const QString &sourcePath, QString &error) {
    const QString destination = installPath();
    if (!QDir().mkpath(QFileInfo(destination).absolutePath())) {
        error = "Could not create the per-user application folder.";
        return false;
    }

    if (QFileInfo(sourcePath).canonicalFilePath() != QFileInfo(destination).canonicalFilePath()) {
        QFile source(sourcePath);
        if (!source.open(QIODevice::ReadOnly)) {
            error = "Could not read the downloaded AppImage.";
            return false;
        }

        QSaveFile target(destination);
        if (!target.open(QIODevice::WriteOnly)) {
            error = "Could not write the installed AppImage.";
            return false;
        }
        while (!source.atEnd()) {
            const QByteArray chunk = source.read(1024 * 1024);
            if (chunk.isEmpty() && source.error() != QFileDevice::NoError) {
                target.cancelWriting();
                error = "An error occurred while reading the downloaded AppImage.";
                return false;
            }
            if (!chunk.isEmpty() && target.write(chunk) != chunk.size()) {
                target.cancelWriting();
                error = "An error occurred while copying the AppImage.";
                return false;
            }
        }
        if (!target.commit()) {
            error = "Could not finish installing the AppImage.";
            return false;
        }
    }

    const auto permissions = QFile::permissions(destination) |
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
        QFileDevice::ReadGroup | QFileDevice::ExeGroup |
        QFileDevice::ReadOther | QFileDevice::ExeOther;
    if (!QFile::setPermissions(destination, permissions)) {
        error = "The AppImage was copied, but could not be marked executable.";
        return false;
    }

    const QString appDir = qEnvironmentVariable("APPDIR");
    const QString registrationScript = appDir + "/usr/share/daisywheel-reamade/register-appimage.py";
    QProcess registerProcess;
    registerProcess.start("python3", {registrationScript, destination});
    if (!registerProcess.waitForStarted() || !registerProcess.waitForFinished(30000) ||
        registerProcess.exitStatus() != QProcess::NormalExit || registerProcess.exitCode() != 0) {
        error = QString::fromLocal8Bit(registerProcess.readAllStandardError()).trimmed();
        if (error.isEmpty()) error = "KDE could not register the keyboard. Make sure Python 3 and kbuildsycoca6 are available.";
        return false;
    }
    return true;
}

void showInstalledMessage() {
    QMessageBox message(QMessageBox::Information, "Daisywheel REAmade installed",
        "Daisywheel REAmade is installed for your user.\n\n"
        "To finish setup, open System Settings → Keyboard → Virtual Keyboard, "
        "select Daisywheel REAmade, and click Apply. Your current keyboard choice is unchanged.",
        QMessageBox::NoButton);
    auto *openSettings = message.addButton("Open Virtual Keyboard Settings", QMessageBox::ActionRole);
    message.addButton("Done", QMessageBox::AcceptRole);
    message.exec();
    if (message.clickedButton() == openSettings && !QProcess::startDetached("kcmshell6", {"kcm_virtualkeyboard"})) {
        QMessageBox::information(nullptr, "Open Virtual Keyboard Settings",
            "Open System Settings manually, then go to Keyboard → Virtual Keyboard.");
    }
}
}

bool runAppImageInstaller() {
    const QString source = qEnvironmentVariable("APPIMAGE");
    if (source.isEmpty()) {
        QMessageBox::critical(nullptr, "Daisywheel REAmade installer",
            "This installer only works when Daisywheel REAmade is launched from its AppImage file.");
        return false;
    }

    QMessageBox prompt(QMessageBox::Information, "Install Daisywheel REAmade",
        "Install Daisywheel REAmade for your user? It will be copied to a stable location and added to KDE's virtual keyboard list.\n\n"
        "You can then select it in System Settings → Keyboard → Virtual Keyboard. The installer will not change your current keyboard choice.",
        QMessageBox::NoButton);
    auto *install = prompt.addButton("Install for this user", QMessageBox::AcceptRole);
    auto *runOnce = prompt.addButton("Run without installing", QMessageBox::ActionRole);
    prompt.addButton("Cancel", QMessageBox::RejectRole);
    prompt.setDefaultButton(qobject_cast<QPushButton *>(install));
    prompt.exec();
    if (prompt.clickedButton() == runOnce) return true;
    if (prompt.clickedButton() != install) return false;

    QString error;
    if (!installImage(source, error)) {
        QMessageBox::critical(nullptr, "Daisywheel REAmade installation failed", error);
        return false;
    }
    showInstalledMessage();
    return false;
}
