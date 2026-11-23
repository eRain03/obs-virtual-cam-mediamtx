#include "virtual_properties.h"
#include "ui_virtual_properties.h"
#include "virtual_output.h"
#include "camera_name.h"
#include "mediamtx_controller.h"
#include <math.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QTextCursor>
#include <QUrl>

VirtualProperties::VirtualProperties(QWidget *parent)
	: QDialog(parent),
	  ui(new Ui::VirtualProperties)
{
	ui->setupUi(this);
	connect(ui->pushButtonStart, SIGNAL(clicked()), this, SLOT(onStart()));
	connect(ui->pushButtonStop, SIGNAL(clicked()), this, SLOT(onStop()));
	connect(ui->spinBox, SIGNAL(valueChanged(int)), ui->horizontalSlider,
		SLOT(setValue(int)));
	connect(ui->horizontalSlider, SIGNAL(valueChanged(int)), ui->spinBox,
		SLOT(setValue(int)));
	connect(ui->checkBox_horiflip, SIGNAL(stateChanged(int)), this,
		SLOT(onClickHorizontalFlip()));
	connect(ui->checkBox_keepratio, SIGNAL(stateChanged(int)), this,
		SLOT(onClickKeepAspectRatio()));

	camera_name = new CameraNameController(this);
	mediamtx = new MediamtxController(this);
	connect(ui->pushButton_applyName, &QPushButton::clicked, this,
		&VirtualProperties::onApplyCameraName);
	connect(camera_name, &CameraNameController::applyFinished, this,
		&VirtualProperties::onCameraNameApplied);
	connect(ui->pushButton_mediaStart, &QPushButton::clicked, this,
		&VirtualProperties::onMediamtxStart);
	connect(ui->pushButton_mediaStop, &QPushButton::clicked, this,
		&VirtualProperties::onMediamtxStop);
	connect(ui->pushButton_openLog, &QPushButton::clicked, this,
		&VirtualProperties::onMediamtxOpenLog);
	connect(mediamtx, &MediamtxController::logText, this,
		&VirtualProperties::onMediamtxLog);
	connect(mediamtx, &MediamtxController::runningChanged, this,
		&VirtualProperties::onMediamtxRunningChanged);

	ui->lineEdit_mediaPath->setText(
		QDir::toNativeSeparators(mediamtx->exePath()));
	ui->lineEdit_mediaPath->setToolTip(
		QDir::toNativeSeparators(mediamtx->configPath()));
	ui->plainTextEdit_log->setMaximumBlockCount(3000);
	ui->plainTextEdit_log->setFont(
		QFontDatabase::systemFont(QFontDatabase::FixedFont));
	onMediamtxRunningChanged(false);

	config_t *config = obs_frontend_get_global_config();
	config_set_default_bool(config, "VirtualOutput", "AutoStart", false);
	config_set_default_bool(config, "VirtualOutput", "HoriFlip", false);
	config_set_default_bool(config, "VirtualOutput", "KeepRatio", false);
	config_set_default_int(config, "VirtualOutput", "OutDelay", 3);
	config_set_default_int(config, "VirtualOutput", "Target", 0);
	bool autostart = config_get_bool(config, "VirtualOutput", "AutoStart");
	bool hori_flip = config_get_bool(config, "VirtualOutput", "HoriFlip");
	bool keep_ratio = config_get_bool(config, "VirtualOutput", "KeepRatio");
	int delay = config_get_int(config, "VirtualOutput", "OutDelay");
	int target = config_get_int(config, "VirtualOutput", "Target");

	ui->checkBox_auto->setChecked(autostart);
	ui->checkBox_horiflip->setChecked(hori_flip);
	ui->checkBox_keepratio->setChecked(keep_ratio);
	ui->comboBox_target->addItem("OBS-Camera", ModeVideo);
	ui->comboBox_target->addItem("OBS-Camera2", ModeVideo2);
	ui->comboBox_target->addItem("OBS-Camera3", ModeVideo3);
	ui->comboBox_target->addItem("OBS-Camera4", ModeVideo4);
	ui->comboBox_target->setCurrentIndex(
		ui->comboBox_target->findData(target));
	RefreshCameraNames();
	ui->spinBox->setValue(delay);
	ui->horizontalSlider->setValue(delay);
	ui->label->setStyleSheet("QLabel { color : red; }");
	ui->label->setVisible(false);
	ui->pushButtonStop->setEnabled(false);
	update_data.keep_ratio = keep_ratio;
	update_data.horizontal_flip = hori_flip;

	if (autostart) {
		obs_get_video_info(&video_info);
		onStart();
	}
}

VirtualProperties::~VirtualProperties()
{
	Shutdown();
	if (mediamtx)
		mediamtx->blockSignals(true);
	if (camera_name)
		camera_name->blockSignals(true);
	virtual_output_disable();
	virtual_output_terminate();
	SaveSetting();
	delete ui;
}

void VirtualProperties::SetVisable()
{
	setVisible(!isVisible());
}

void VirtualProperties::EnableOptions(bool enable)
{
	ui->spinBox->setEnabled(enable);
	ui->horizontalSlider->setEnabled(enable);
	ui->comboBox_target->setEnabled(enable);
	ui->pushButtonStart->setEnabled(enable);
	ui->pushButtonStop->setEnabled(!enable);
}

void VirtualProperties::ShowWarning(bool show)
{
	ui->label->setVisible(show);
}

void VirtualProperties::onStart()
{
	ShowWarning(false);
	UpdateParameter();
	EnableOptions(false);
	virtual_signal_connect("output_stop", onStopSignal, this);
	virtual_output_enable();
}

void VirtualProperties::onStop()
{
	virtual_output_disable();
}

void VirtualProperties::onStopSignal(void *data, calldata_t *cd)
{
	auto page = (VirtualProperties *)data;
	bool start_fail = calldata_bool(cd, "start_fail");
	if (start_fail)
		page->ShowWarning(true);
	page->EnableOptions(true);
	virtual_signal_disconnect("output_stop", onStopSignal, data);
}

void VirtualProperties::UpdateParameter()
{

	update_data.horizontal_flip = ui->checkBox_horiflip->isChecked();
	update_data.keep_ratio = ui->checkBox_keepratio->isChecked();
	update_data.delay = ui->horizontalSlider->value();
	update_data.mode = ui->comboBox_target->currentData().toInt();
	virtual_output_set_data(&update_data);
}

void VirtualProperties::onClickHorizontalFlip()
{
	UpdateParameter();
}

void VirtualProperties::onClickKeepAspectRatio()
{
	UpdateParameter();
}

void VirtualProperties::Shutdown()
{
	if (mediamtx)
		mediamtx->stop();
}

void VirtualProperties::onApplyCameraName()
{
	const QString name = ui->lineEdit_cameraName->text().trimmed();
	if (!CameraNameController::isValidName(name)) {
		ui->label_cameraStatus->setText(
			QStringLiteral("Name must be 1-60 characters without "
				       "quotes, backslashes or control "
				       "characters."));
		return;
	}

	ui->pushButton_applyName->setEnabled(false);
	ui->lineEdit_cameraName->setEnabled(false);
	ui->label_cameraStatus->setText(
		QStringLiteral("Waiting for administrator approval..."));
	camera_name->apply(name);
}

void VirtualProperties::onCameraNameApplied(bool success,
					    const QString &message)
{
	ui->pushButton_applyName->setEnabled(true);
	ui->lineEdit_cameraName->setEnabled(true);

	const QString expected = ui->lineEdit_cameraName->text().trimmed();
	if (success) {
		RefreshCameraNames();
		if (CameraNameController::currentName(0) == expected) {
			ui->label_cameraStatus->setText(QStringLiteral(
				"Camera name updated. Reopen "
				"applications that use the camera to "
				"see the new name."));
		} else {
			ui->label_cameraStatus->setText(QStringLiteral(
				"Registration succeeded but the new "
				"name could not be read back."));
		}
	} else if (message == QStringLiteral("cancelled")) {
		ui->label_cameraStatus->setText(QStringLiteral(
			"Administrator permission was cancelled."));
	} else {
		ui->label_cameraStatus->setText(
			QStringLiteral("Failed to update camera name: %1")
				.arg(message));
	}
}

void VirtualProperties::onMediamtxStart()
{
	QString error;
	if (!mediamtx->start(&error)) {
		ui->label_mediaStatus->setText(
			QStringLiteral("Failed to start"));
		onMediamtxLog(QStringLiteral("Failed to start MediaMTX: %1\n")
				      .arg(error));
	}
}

void VirtualProperties::onMediamtxStop()
{
	mediamtx->stop();
}

void VirtualProperties::onMediamtxOpenLog()
{
	const QString path = mediamtx->logFilePath();
	const QFileInfo info(path);
	QDesktopServices::openUrl(QUrl::fromLocalFile(
		info.exists() ? path : info.absolutePath()));
}

void VirtualProperties::onMediamtxLog(const QString &text)
{
	ui->plainTextEdit_log->moveCursor(QTextCursor::End);
	ui->plainTextEdit_log->insertPlainText(text);
	ui->plainTextEdit_log->moveCursor(QTextCursor::End);
}

void VirtualProperties::onMediamtxRunningChanged(bool running)
{
	ui->pushButton_mediaStart->setEnabled(!running);
	ui->pushButton_mediaStop->setEnabled(running);
	ui->label_mediaStatus->setText(running ? QStringLiteral("Running")
					       : QStringLiteral("Stopped"));
}

void VirtualProperties::showEvent(QShowEvent *event)
{
	RefreshCameraNames();
	UNUSED_PARAMETER(event);
}

void VirtualProperties::RefreshCameraNames()
{
	for (int i = 0; i < ui->comboBox_target->count(); ++i)
		ui->comboBox_target->setItemText(
			i, CameraNameController::currentName(i));

	if (!camera_name->isBusy())
		ui->lineEdit_cameraName->setText(
			CameraNameController::currentName(0));
}

void VirtualProperties::closeEvent(QCloseEvent *event)
{
	SaveSetting();
	UNUSED_PARAMETER(event);
}

void VirtualProperties::SaveSetting()
{
	config_t *config = obs_frontend_get_global_config();
	if (config) {
		bool autostart = ui->checkBox_auto->isChecked();
		bool hori_flip = ui->checkBox_horiflip->isChecked();
		bool keep_ratio = ui->checkBox_keepratio->isChecked();
		int delay = ui->horizontalSlider->value();
		config_set_bool(config, "VirtualOutput", "AutoStart",
				autostart);
		config_set_bool(config, "VirtualOutput", "HoriFlip", hori_flip);
		config_set_bool(config, "VirtualOutput", "KeepRatio",
				keep_ratio);
		config_set_int(config, "VirtualOutput", "OutDelay", delay);
	}
}