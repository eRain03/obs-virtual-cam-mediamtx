#include "mediamtx_controller.h"

#include <obs-module.h>
#include <util/bmem.h>

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static QString moduleFile(const char *file)
{
	char *path = obs_module_file(file);
	if (!path)
		return QString();
	const QString result = QString::fromUtf8(path);
	bfree(path);
	return result;
}

static QString dataRoot()
{
	QString base = qEnvironmentVariable("APPDATA");
	if (base.isEmpty())
		base = QStandardPaths::writableLocation(
			QStandardPaths::AppDataLocation);
	if (base.isEmpty())
		base = QDir::homePath();
	return base + QStringLiteral("/obs-virtualcam");
}

static QString defaultLogFilePath()
{
	const QString dir = dataRoot() + QStringLiteral("/logs");
	QDir().mkpath(dir);
	return dir + QStringLiteral("/mediamtx.log");
}

// The bundled configuration lives in the (possibly read-only) install tree.
// Keep a copy in AppData so users can edit it and MediaMTX has a writable
// working directory for relative paths.
static QString userConfigPath(const QString &bundled)
{
	const QString dir = dataRoot() + QStringLiteral("/mediamtx");
	QDir().mkpath(dir);
	const QString path = dir + QStringLiteral("/mediamtx.yml");
	if (!QFileInfo::exists(path) && QFileInfo::exists(bundled)) {
		if (!QFile::copy(bundled, path))
			return bundled;
	}
	return path;
}

MediamtxController::MediamtxController(QObject *parent) : QObject(parent)
{
	exe = moduleFile("mediamtx/mediamtx.exe");
	config = userConfigPath(moduleFile("mediamtx/mediamtx.yml"));
	logPath = defaultLogFilePath();

	process = new QProcess(this);
	process->setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
	process->setCreateProcessArgumentsModifier(
		[](QProcess::CreateProcessArguments *args) {
			args->flags |= CREATE_NO_WINDOW;
			args->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
			args->startupInfo->wShowWindow = SW_HIDE;
		});
#endif

	connect(process, &QProcess::readyReadStandardOutput, this,
		[this] { drainOutput(); });

	connect(process,
		QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		this, [this](int code, QProcess::ExitStatus status) {
			drainOutput();
			appendLog(
				QStringLiteral("exited (code %1, %2)")
					.arg(code)
					.arg(status == QProcess::NormalExit
						     ? QStringLiteral("normal")
						     : QStringLiteral(
							       "crashed")));
			cleanupJob();
			if (logFile.isOpen())
				logFile.close();
			emit runningChanged(false);
		});

	connect(process, &QProcess::errorOccurred, this,
		[this](QProcess::ProcessError) {
			appendLog(QStringLiteral("process error: %1")
					  .arg(process->errorString()));
		});
}

MediamtxController::~MediamtxController()
{
	blockSignals(true);
	disconnect(process, nullptr, this, nullptr);
	stop();
}

bool MediamtxController::isRunning() const
{
	return process && process->state() != QProcess::NotRunning;
}

bool MediamtxController::start(QString *error)
{
	if (isRunning())
		return true;

	if (!QFileInfo::exists(exe)) {
		if (error)
			*error = QStringLiteral(
					 "MediaMTX executable not found: %1")
					 .arg(exe);
		return false;
	}
	if (!QFileInfo::exists(config)) {
		if (error)
			*error = QStringLiteral(
					 "MediaMTX configuration not found: %1")
					 .arg(config);
		return false;
	}

	if (!logFile.isOpen()) {
		logFile.setFileName(logPath);
		if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append |
				  QIODevice::Text)) {
			if (error)
				*error = QStringLiteral(
						 "Cannot open log file: %1")
						 .arg(logPath);
			return false;
		}
	}

	appendLog(QStringLiteral("=== starting %1 ===")
			  .arg(QDateTime::currentDateTime().toString(
				  Qt::ISODate)));
	appendLog(QStringLiteral("program: %1").arg(exe));
	appendLog(QStringLiteral("config: %1").arg(config));

	process->setWorkingDirectory(QFileInfo(config).absolutePath());
	process->setProgram(exe);
	// MediaMTX takes the configuration file as a positional argument.
	process->setArguments(QStringList{config});
	process->start();

	if (!process->waitForStarted(5000)) {
		const QString message = process->errorString();
		appendLog(QStringLiteral("failed to start: %1").arg(message));
		if (error)
			*error = message;
		if (logFile.isOpen())
			logFile.close();
		return false;
	}

	attachJob();
	appendLog(QStringLiteral("started (pid %1)").arg(process->processId()));
	emit runningChanged(true);
	return true;
}

void MediamtxController::stop()
{
	if (!process)
		return;

	if (process->state() == QProcess::NotRunning) {
		cleanupJob();
		if (logFile.isOpen())
			logFile.close();
		return;
	}

	appendLog(QStringLiteral("stopping"));
	process->terminate();
	if (!process->waitForFinished(2000)) {
		process->kill();
		process->waitForFinished(2000);
	}
	drainOutput();
	cleanupJob();
	appendLog(QStringLiteral("stopped"));
	if (logFile.isOpen())
		logFile.close();
	emit runningChanged(false);
}

void MediamtxController::drainOutput()
{
	if (!process)
		return;

	const QByteArray data = process->readAllStandardOutput();
	if (data.isEmpty())
		return;

	if (logFile.isOpen()) {
		logFile.write(data);
		logFile.flush();
	}
	emit logText(QString::fromUtf8(data));
}

void MediamtxController::appendLog(const QString &text)
{
	const QString line = QStringLiteral("[%1] %2\n")
				     .arg(QDateTime::currentDateTime().toString(
						  QStringLiteral("hh:mm:ss")),
					  text);
	if (logFile.isOpen()) {
		logFile.write(line.toUtf8());
		logFile.flush();
	}
	emit logText(line);
}

void MediamtxController::attachJob()
{
#ifdef Q_OS_WIN
	jobHandle = CreateJobObjectW(nullptr, nullptr);
	if (!jobHandle)
		return;

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
	info.BasicLimitInformation.LimitFlags =
		JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	SetInformationJobObject(static_cast<HANDLE>(jobHandle),
				JobObjectExtendedLimitInformation, &info,
				sizeof(info));

	HANDLE child = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE,
				   static_cast<DWORD>(process->processId()));
	if (child) {
		AssignProcessToJobObject(static_cast<HANDLE>(jobHandle), child);
		CloseHandle(child);
	}
#endif
}

void MediamtxController::cleanupJob()
{
#ifdef Q_OS_WIN
	if (jobHandle) {
		CloseHandle(static_cast<HANDLE>(jobHandle));
		jobHandle = nullptr;
	}
#endif
}
