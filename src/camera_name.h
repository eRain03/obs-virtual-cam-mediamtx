#pragma once

#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

class QTimer;

// Reads the DirectShow camera display name from the registry and re-registers
// the camera under a new name. Registration lives in HKCR, so the change is
// performed by regsvr32 through a UAC elevation prompt.
class CameraNameController : public QObject {
	Q_OBJECT

public:
	explicit CameraNameController(QObject *parent = nullptr);
	~CameraNameController() override;

	static QString currentName(int mode = 0);
	static bool isValidName(const QString &name);

	bool isBusy() const { return applying; }
	void apply(const QString &name);

signals:
	void applyFinished(bool success, const QString &message);

private:
	void startNextTarget();
	void pollProcess();
	void finish(bool success, const QString &message);

	QList<QPair<QString, QString>> targets; // dll path, regsvr32 path
	int targetIndex = 0;
	bool applying = false;
	bool hadError = false;
	QString cameraName;
	void *processHandle = nullptr;
	QTimer *pollTimer = nullptr;
};
