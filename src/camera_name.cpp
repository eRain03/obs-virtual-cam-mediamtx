#include "camera_name.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QTimer>

#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

// CLSIDs of the four video filters, in registration order.
static const wchar_t *kClsidKeys[4] = {
	L"CLSID\\{27B05C2D-93DC-474A-A5DA-9BBA34CB2A9C}",
	L"CLSID\\{27B05C2D-93DC-474A-A5DA-9BBA34CB2A9D}",
	L"CLSID\\{27B05C2D-93DC-474A-A5DA-9BBA34CB2A9E}",
	L"CLSID\\{27B05C2D-93DC-474A-A5DA-9BBA34CB2A9F}",
};

// Moniker entries created by IFilterMapper2; FriendlyName is what apps show.
static const wchar_t *kInstanceKeys[4] = {
	L"CLSID\\{860BB310-5D01-11D0-BD3B-00A0C911CE86}\\Instance\\{27B05C2D-93DC-474A-A5DA-9BBA34CB2A9C}",
	L"CLSID\\{860BB310-5D01-11D0-BD3B-00A0C911CE86}\\Instance\\{27B05C2D-93DC-474A-A5DA-9BBA34CB2A9D}",
	L"CLSID\\{860BB310-5D01-11D0-BD3B-00A0C911CE86}\\Instance\\{27B05C2D-93DC-474A-A5DA-9BBA34CB2A9E}",
	L"CLSID\\{860BB310-5D01-11D0-BD3B-00A0C911CE86}\\Instance\\{27B05C2D-93DC-474A-A5DA-9BBA34CB2A9F}",
};

static int g_moduleMarker = 0;

static HMODULE currentModule()
{
	HMODULE mod = nullptr;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
				   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			   reinterpret_cast<LPCWSTR>(&g_moduleMarker), &mod);
	return mod;
}

static QString modulePath()
{
	HMODULE mod = currentModule();
	if (!mod)
		return QString();

	WCHAR buf[MAX_PATH] = {};
	if (!GetModuleFileNameW(mod, buf, MAX_PATH))
		return QString();
	return QString::fromWCharArray(buf);
}

// OBS install root, derived from the plugin location
// (<root>\obs-plugins\64bit\obs-virtualcam.dll or <root>\bin\64bit\...).
static QString obsRoot()
{
	const QString path = modulePath();
	const int idx = path.lastIndexOf(QStringLiteral("\\obs-plugins\\"), -1,
					 Qt::CaseInsensitive);
	if (idx > 0)
		return path.left(idx);

	QDir dir = QFileInfo(path).absoluteDir();
	if (dir.cdUp() && dir.cdUp())
		return dir.absolutePath();
	return QString();
}

static QString systemRoot()
{
	QString root = qEnvironmentVariable("SystemRoot");
	if (root.isEmpty())
		root = QStringLiteral("C:\\Windows");
	return root;
}

static void appendTarget(QList<QPair<QString, QString>> &targets,
			 QSet<QString> &seen, const QString &dll,
			 const QString &regsvr32)
{
	if (!QFileInfo::exists(dll))
		return;
	const QString key = QDir::cleanPath(dll).toLower();
	if (seen.contains(key))
		return;
	seen.insert(key);
	targets.append(qMakePair(QDir::cleanPath(dll), regsvr32));
}

static QList<QPair<QString, QString>> findRegistrationTargets()
{
	const QString root = obsRoot();
	const QString sys = systemRoot();
	const QString regsvr32_64 =
		sys + QStringLiteral("\\System32\\regsvr32.exe");
	const QString regsvr32_32 =
		sys + QStringLiteral("\\SysWOW64\\regsvr32.exe");

	QList<QPair<QString, QString>> targets;
	QSet<QString> seen;
	appendTarget(
		targets, seen,
		root + QStringLiteral("\\bin\\64bit\\obs-virtualsource.dll"),
		regsvr32_64);
	appendTarget(
		targets, seen,
		root + QStringLiteral(
			       "\\obs-plugins\\64bit\\obs-virtualsource.dll"),
		regsvr32_64);
	appendTarget(
		targets, seen,
		root + QStringLiteral("\\bin\\32bit\\obs-virtualsource.dll"),
		regsvr32_32);
	appendTarget(
		targets, seen,
		root + QStringLiteral(
			       "\\obs-plugins\\32bit\\obs-virtualsource.dll"),
		regsvr32_32);
	appendTarget(targets, seen,
		     QFileInfo(modulePath())
			     .absoluteDir()
			     .filePath(QStringLiteral("obs-virtualsource.dll")),
		     regsvr32_64);
	return targets;
}

CameraNameController::CameraNameController(QObject *parent) : QObject(parent)
{
	pollTimer = new QTimer(this);
	pollTimer->setInterval(100);
	connect(pollTimer, &QTimer::timeout, this,
		&CameraNameController::pollProcess);
}

CameraNameController::~CameraNameController()
{
	if (processHandle) {
		CloseHandle(static_cast<HANDLE>(processHandle));
		processHandle = nullptr;
	}
}

QString CameraNameController::currentName(int mode)
{
	if (mode < 0 || mode > 3)
		mode = 0;

	WCHAR buf[256] = {};
	DWORD size = sizeof(buf);
	if (RegGetValueW(HKEY_CLASSES_ROOT, kInstanceKeys[mode],
			 L"FriendlyName", RRF_RT_REG_SZ, nullptr, buf,
			 &size) == ERROR_SUCCESS &&
	    buf[0])
		return QString::fromWCharArray(buf);

	size = sizeof(buf);
	if (RegGetValueW(HKEY_CLASSES_ROOT, kClsidKeys[mode], nullptr,
			 RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS &&
	    buf[0])
		return QString::fromWCharArray(buf);

	return mode == 0 ? QStringLiteral("OBS-Camera")
			 : QStringLiteral("OBS-Camera%1").arg(mode + 1);
}

bool CameraNameController::isValidName(const QString &name)
{
	const QString trimmed = name.trimmed();
	if (trimmed.isEmpty() || trimmed.size() > 60)
		return false;

	for (const QChar c : trimmed) {
		// Quotes and trailing backslashes would break the regsvr32 command
		// line, control characters cannot be represented in the registry.
		if (c.unicode() == u'"' || c.unicode() == u'\\' ||
		    c.unicode() < 0x20)
			return false;
	}
	return true;
}

void CameraNameController::apply(const QString &name)
{
	if (applying || !isValidName(name))
		return;

	cameraName = name.trimmed();
	targets = findRegistrationTargets();
	if (targets.isEmpty()) {
		emit applyFinished(
			false,
			QStringLiteral("obs-virtualsource.dll not found"));
		return;
	}

	targetIndex = 0;
	hadError = false;
	applying = true;
	startNextTarget();
}

void CameraNameController::startNextTarget()
{
	if (targetIndex >= targets.size()) {
		finish(!hadError,
		       hadError ? QStringLiteral("registration failed")
				: QString());
		return;
	}

	const auto &target = targets.at(targetIndex);
	const std::wstring file = target.second.toStdWString();
	const QString params =
		QStringLiteral("/s /i:\"apply:%1\" \"%2\"")
			.arg(cameraName,
			     QDir::toNativeSeparators(target.first));
	const std::wstring paramsW = params.toStdWString();

	SHELLEXECUTEINFOW sei = {};
	sei.cbSize = sizeof(sei);
	sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
	sei.lpVerb = L"runas";
	sei.lpFile = file.c_str();
	sei.lpParameters = paramsW.c_str();
	sei.nShow = SW_HIDE;

	if (!ShellExecuteExW(&sei)) {
		const DWORD err = GetLastError();
		if (err == ERROR_CANCELLED) {
			finish(false, QStringLiteral("cancelled"));
			return;
		}
		hadError = true;
		++targetIndex;
		startNextTarget();
		return;
	}

	processHandle = sei.hProcess;
	pollTimer->start();
}

void CameraNameController::pollProcess()
{
	if (!processHandle)
		return;

	const HANDLE handle = static_cast<HANDLE>(processHandle);
	if (WaitForSingleObject(handle, 0) != WAIT_OBJECT_0)
		return;

	DWORD exitCode = 1;
	GetExitCodeProcess(handle, &exitCode);
	CloseHandle(handle);
	processHandle = nullptr;
	pollTimer->stop();

	if (exitCode != 0)
		hadError = true;

	++targetIndex;
	startNextTarget();
}

void CameraNameController::finish(bool success, const QString &message)
{
	applying = false;
	if (pollTimer)
		pollTimer->stop();
	if (processHandle) {
		CloseHandle(static_cast<HANDLE>(processHandle));
		processHandle = nullptr;
	}
	emit applyFinished(success, message);
}
