#pragma once

#include <QFile>
#include <QObject>
#include <QString>

class QProcess;

// Runs the bundled MediaMTX binary as a hidden child process, forwards its
// output to both the UI and a log file, and makes sure it does not outlive
// OBS (job object with kill-on-close).
class MediamtxController : public QObject {
	Q_OBJECT

public:
	explicit MediamtxController(QObject *parent = nullptr);
	~MediamtxController() override;

	bool isRunning() const;
	QString exePath() const { return exe; }
	QString configPath() const { return config; }
	QString logFilePath() const { return logPath; }

	bool start(QString *error);
	void stop();

signals:
	void logText(const QString &text);
	void runningChanged(bool running);

private:
	void appendLog(const QString &text);
	void drainOutput();
	void attachJob();
	void cleanupJob();

	QProcess *process = nullptr;
	QFile logFile;
	QString exe;
	QString config;
	QString logPath;
	void *jobHandle = nullptr;
};
