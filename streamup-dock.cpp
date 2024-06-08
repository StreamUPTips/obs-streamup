#include "streamup-dock.hpp"
#include "ui_StreamupDock.h"
#include <QMainWindow>
#include <QDockWidget>
#include "streamup.hpp"
#include <obs-frontend-api.h>

StreamupDock::StreamupDock(QWidget *parent)
	: QWidget(parent), ui(new Ui::StreamupDock)
{
	ui->setupUi(this);

	connect(ui->button1, &QPushButton::clicked, this,
		&StreamupDock::onButton1Clicked);
	connect(ui->button2, &QPushButton::clicked, this,
		&StreamupDock::onButton2Clicked);
	connect(ui->button3, &QPushButton::clicked, this,
		&StreamupDock::onButton3Clicked);
	connect(ui->button4, &QPushButton::clicked, this,
		&StreamupDock::onButton4Clicked);
}

StreamupDock::~StreamupDock()
{
	delete ui;
}

void StreamupDock::onButton1Clicked()
{
	ToggleLockAllSources();
}

void StreamupDock::onButton2Clicked()
{
	ToggleLockSourcesInCurrentScene();
}

void StreamupDock::onButton3Clicked()
{
	obs_enum_sources(RefreshAudioMonitoring, nullptr);
}

void StreamupDock::onButton4Clicked()
{
	obs_enum_sources(RefreshBrowserSources, nullptr);
}
