#ifndef VIRTUAL_PROPERTIES_H
#define VIRTUAL_PROPERTIES_H

#include <QDialog>
#include <QString>
#include "obs.h"
#include "queue/share_queue_write.h"

class CameraNameController;
class MediamtxController;

struct vcam_update_data {
	bool horizontal_flip = false;
	bool keep_ratio = false;
	int delay = 0;
	int mode = 0;
};

namespace Ui {
class VirtualProperties;
}

class VirtualProperties : public QDialog {
	Q_OBJECT

public:
	explicit VirtualProperties(QWidget *parent = 0);
	~VirtualProperties();
	void SetVisable();
	void showEvent(QShowEvent *event);
	void closeEvent(QCloseEvent *event);
	void EnableOptions(bool enable);
	void ShowWarning(bool show);
	void Shutdown();

private Q_SLOTS:
	void onStart();
	void onStop();
	void onClickHorizontalFlip();
	void onClickKeepAspectRatio();
	void onApplyCameraName();
	void onCameraNameApplied(bool success, const QString &message);
	void onMediamtxStart();
	void onMediamtxStop();
	void onMediamtxOpenLog();
	void onMediamtxLog(const QString &text);
	void onMediamtxRunningChanged(bool running);

private:
	Ui::VirtualProperties *ui;
	struct obs_video_info video_info;
	struct vcam_update_data update_data;
	QString scene_name;
	CameraNameController *camera_name = nullptr;
	MediamtxController *mediamtx = nullptr;
	void UpdateParameter();
	void SaveSetting();
	void RefreshCameraNames();
	static void onStopSignal(void *data, calldata_t *cd);
};

#endif // VIRTUAL_PROPERTIES_H
