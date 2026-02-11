#include <obs-data.h>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QAction>
#include <QMenuBar>
#include <QMenu>
#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTimer>
#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QPainter>
#include <QUrl>
#include "qrcodegen/qrcodegen.hpp"
#include "bilibili_api.hpp"
#include "plugin_utils.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

class BilibiliStreamPlugin : public QObject {
    Q_OBJECT
public:
    explicit BilibiliStreamPlugin(QMainWindow* parent) : QObject(parent) {
        menuBar = parent->menuBar();
        setupMenu();
        loadConfig();
        onLoginStatusTriggered();
        if (config.streaming) {
            streamAction->setText("停止直播");
        }
    }

    ~BilibiliStreamPlugin() {
        obs_log(LOG_DEBUG, "释放 BilibiliStreamPlugin 资源");
    }

private:
    Bili::Config config;
    QMenuBar* menuBar;
    QAction* loginStatusAction = nullptr;
    QAction* streamAction = nullptr;

    void openLiveRoom() {
        if (config.room_id.empty()) {
            showMessageDialog("room_id 为空，请先登录或更新直播间信息", "消息");
            return;
        }

        const QString url = QString("https://live.bilibili.com/%1").arg(QString::fromStdString(config.room_id));
        if (!QDesktopServices::openUrl(QUrl(url))) {
            showMessageDialog(QStringLiteral("无法打开浏览器，请手动访问：\n") + url, "消息");
        }
    }

    void setupMenu() {
        QMenu* bilibiliMenu = menuBar->addMenu("Bilibili直播");
        QMenu* loginMenu = bilibiliMenu->addMenu("登录");
        QAction* scanQrcode = loginMenu->addAction("扫码登录");
        loginStatusAction = loginMenu->addAction("登录状态: 未登录");
        loginStatusAction->setCheckable(true);
        loginStatusAction->setEnabled(false);
        streamAction = bilibiliMenu->addAction("开始直播");
        QAction* openRoom = bilibiliMenu->addAction("打开直播间");
        QAction* updateRoomInfo = bilibiliMenu->addAction("更新直播间信息");

        connect(scanQrcode, &QAction::triggered, this, &BilibiliStreamPlugin::onScanQrcodeTriggered);
        connect(loginStatusAction, &QAction::triggered, this, &BilibiliStreamPlugin::onLoginStatusTriggered);
        connect(streamAction, &QAction::triggered, this, &BilibiliStreamPlugin::onStreamButtonTriggered);
        connect(openRoom, &QAction::triggered, this, [this]() { openLiveRoom(); });
        connect(updateRoomInfo, &QAction::triggered, this, &BilibiliStreamPlugin::onUpdateRoomInfoTriggered);
    }

    void loadConfig() {
	    std::string message;
        char* config_file = obs_module_file("config.json");
        if (config_file) {
            obs_data_t* settings = obs_data_create_from_json_file(config_file);
            if (settings) {
                config.room_id = obs_data_get_string(settings, "room_id");
                config.csrf_token = obs_data_get_string(settings, "csrf_token");
                config.cookies = obs_data_get_string(settings, "cookies");
                config.title = obs_data_get_string(settings, "title");
                config.rtmp_addr = obs_data_get_string(settings, "rtmp_addr");
                config.rtmp_code = obs_data_get_string(settings, "rtmp_code");
                config.part_id = obs_data_get_int(settings, "part_id");
                config.area_id = obs_data_get_int(settings, "area_id");
                obs_data_release(settings);
                if (!config.cookies.empty()) {
                    if (Bili::BiliApi::checkLoginStatus(config.cookies, message)) {
                        config.login_status = true;
                        std::string new_room_id, new_csrf_token;
                        if (Bili::BiliApi::getRoomIdAndCsrf(config.cookies, new_room_id, new_csrf_token, message)) {
                            config.room_id = new_room_id;
                            config.csrf_token = new_csrf_token;
                        }
		            } else {
			            showMessageDialog(QString::fromUtf8(message), "消息");
                    }
                }
            }
            bfree(config_file);
        }
        if (config.room_id.empty()) config.room_id = "12345";
        if (config.csrf_token.empty()) config.csrf_token = "your_csrf_token";
        if (config.title.empty()) config.title = "我的直播";
    }

    void saveConfig() {
        char* config_file = obs_module_file("config.json");
        if (config_file) {
            obs_data_t* settings = obs_data_create();
            obs_data_set_string(settings, "room_id", config.room_id.c_str());
            obs_data_set_string(settings, "csrf_token", config.csrf_token.c_str());
            obs_data_set_string(settings, "cookies", config.cookies.c_str());
            obs_data_set_string(settings, "title", config.title.c_str());
            obs_data_set_string(settings, "rtmp_addr", config.rtmp_addr.c_str());
            obs_data_set_string(settings, "rtmp_code", config.rtmp_code.c_str());
            obs_data_set_int(settings, "part_id", config.part_id);
            obs_data_set_int(settings, "area_id", config.area_id);
            obs_data_save_json(settings, config_file);
            obs_data_release(settings);
            bfree(config_file);
        }
    }

    static QPixmap generateQrCode(const std::string& qr_data) {
        if (qr_data.empty()) {
            obs_log(LOG_ERROR, "二维码数据为空");
            return QPixmap();
        }
        using qrcodegen::QrCode;
        const QrCode qr = QrCode::encodeText(qr_data.c_str(), QrCode::Ecc::LOW);
        if (qr.getSize() <= 0) {
            obs_log(LOG_ERROR, "生成二维码失败");
            return QPixmap();
        }
        const int scale = 10;
		const int border = 4;
        const int size = qr.getSize();
        QImage image((size + border * 2) * scale, (size +border * 2 ) * scale, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                if (qr.getModule(x, y)) {
                    painter.drawRect((x + border )* scale, (y + border ) * scale, scale, scale);
                }
            }
        }
        return QPixmap::fromImage(image);
    }

public slots:
    void onScanQrcodeTriggered() {
        std::string qr_data, qr_key, message;
        if (!Bili::BiliApi::getQrCode(config.cookies, qr_data, qr_key, message)) {

            obs_log(LOG_ERROR, "获取二维码失败");
		    showMessageDialog(QString::fromUtf8(message), "消息");
            return;
        }

        QDialog* qrDialog = new QDialog((QWidget*)obs_frontend_get_main_window());
        qrDialog->setWindowTitle("Bilibili 登录二维码");
        QVBoxLayout* layout = new QVBoxLayout(qrDialog);
        QLabel* qrLabel = new QLabel(qrDialog);
        QPixmap qrPixmap = generateQrCode(qr_data);
        if (qrPixmap.isNull()) {
            qrLabel->setText("无法生成二维码");
        } else {
            qrLabel->setPixmap(qrPixmap);
        }
        layout->addWidget(qrLabel);
        layout->addWidget(new QLabel("使用手机扫描二维码登录", qrDialog));
        qrDialog->setLayout(layout);

        QTimer* timer = new QTimer(qrDialog);
		int* retryCount = new int(0);
        connect(timer, &QTimer::timeout, [=]() mutable {
            std::string cookies;

			if (*retryCount > 180) {
		        timer->stop();
		        qrLabel->setText("二维码已过期，请重新打开");
		        // 或者直接关闭窗口 qrDialog->reject();
		        delete retryCount; 
		        return;
		    }
			
            if (Bili::BiliApi::qrLogin(qr_key, cookies, message)) {
                timer->stop();
                config.cookies = cookies;
                config.login_status = Bili::BiliApi::checkLoginStatus(config.cookies, message);
                onLoginStatusTriggered();
                std::string new_room_id, new_csrf_token;
                if (Bili::BiliApi::getRoomIdAndCsrf(config.cookies, new_room_id, new_csrf_token, message)) {
                    config.room_id = new_room_id;
                    config.csrf_token = new_csrf_token;
                }
                saveConfig();
                qrDialog->accept();
		    //} else {
		    //    showMessageDialog(QString::fromUtf8(message), "消息");
            }
        });
        timer->start(1000);
        connect(qrDialog, &QDialog::finished, [=]() { qrDialog->deleteLater(); });
        qrDialog->exec();
    }

    void onLoginStatusTriggered() {
        loginStatusAction->setText(config.login_status ? "登录状态: 已登录" : "登录状态: 未登录");
        loginStatusAction->setChecked(config.login_status);
    }

    void onStreamButtonTriggered() {
	    std::string message;
        if (config.streaming) {
            if (Bili::BiliApi::stopLive(config, message)) {
                streamAction->setText("开始直播");
                config.streaming = false;
                showMessageDialog("直播已停止", "消息");
            } else {
		        showMessageDialog(QString::fromUtf8(message), "消息");
            }
        } else if (!config.area_id) {
            showMessageDialog("请更新直播间分区", "消息");
        } else {
            std::string rtmp_addr, rtmp_code, face_qr;
            if (Bili::BiliApi::startLive(config, rtmp_addr, rtmp_code, message, face_qr)) {
                streamAction->setText("停止直播");
                config.streaming = true;
                config.rtmp_addr = rtmp_addr;
                config.rtmp_code = rtmp_code;
                saveConfig();
                QDialog* dialog = new QDialog((QWidget*)obs_frontend_get_main_window());
                dialog->setWindowTitle("消息");
                QVBoxLayout* layout = new QVBoxLayout(dialog);
                layout->addWidget(new QLabel(QString("Bilibili已开始直播请复制以下内容进行推流\n"
                                                    "若自定义推流失败请使用预设的B站推流更换推流地址尝试\n"
                                                    "RTMP 地址: %1\n推流码: %2").arg(rtmp_addr.c_str(), rtmp_code.c_str()), dialog));
                QPushButton* copy = new QPushButton("复制", dialog);
                QPushButton* confirm = new QPushButton("确认", dialog);
                layout->addWidget(copy);
                layout->addWidget(confirm);
                connect(copy, &QPushButton::clicked, [=]() {
                    QApplication::clipboard()->setText(QString("推流地址: %1\n推流码: %2").arg(rtmp_addr.c_str(), rtmp_code.c_str()));
                });
                connect(confirm, &QPushButton::clicked, [=]() { dialog->accept(); });
                connect(dialog, &QDialog::finished, [=]() { dialog->deleteLater(); });
                dialog->exec();
	        } else {
		        if (!face_qr.empty()) {
			        QDialog *faceDialog = new QDialog((QWidget *)obs_frontend_get_main_window());
			        faceDialog->setWindowTitle("实名认证（人脸识别）");
			        QVBoxLayout *layout = new QVBoxLayout(faceDialog);

			        // 生成并显示二维码
			        QLabel *qrLabel = new QLabel(faceDialog);
			        QPixmap qrPixmap = generateQrCode(face_qr);
			        if (qrPixmap.isNull()) {
				        qrLabel->setText("二维码生成失败，请检查网络或日志");
			        } else {
				        qrLabel->setPixmap(qrPixmap);
				        qrLabel->setAlignment(Qt::AlignCenter);
			        }

			        QLabel *tipLabel =
				        new QLabel("请使用<b>手机 Bilibili App</b> 扫描下方二维码完成人脸认证。<br>"
					           "认证完成后，请重新点击“开始直播”。",
					           faceDialog);
			        tipLabel->setWordWrap(true);
			        tipLabel->setAlignment(Qt::AlignCenter);

			        QPushButton *closeBtn = new QPushButton("我已完成认证", faceDialog);

			        layout->addWidget(tipLabel);
			        layout->addWidget(qrLabel);
			        layout->addWidget(closeBtn);

			        connect(closeBtn, &QPushButton::clicked, faceDialog, &QDialog::accept);
			        connect(faceDialog, &QDialog::finished, faceDialog, &QDialog::deleteLater);

			        faceDialog->exec();
		        } else {
			        // 其他类型的开播失败错误
			        showMessageDialog(QString::fromUtf8(message.c_str()), "开播失败");
		        }
            }
        }
    }

    void onUpdateRoomInfoTriggered() {
	    std::string message;
        QDialog* dialog = new QDialog((QWidget*)obs_frontend_get_main_window());
        dialog->setWindowTitle("更新直播间信息");
        QVBoxLayout* layout = new QVBoxLayout(dialog);

        QHBoxLayout* openRoomLayout = new QHBoxLayout();
        QLabel* roomLabel = new QLabel(QString("直播间: https://live.bilibili.com/%1")
                                           .arg(QString::fromStdString(config.room_id)),
                                       dialog);
        QPushButton* openRoomBtn = new QPushButton("打开直播间", dialog);
        openRoomLayout->addWidget(roomLabel);
        openRoomLayout->addWidget(openRoomBtn);
        layout->addLayout(openRoomLayout);
        connect(openRoomBtn, &QPushButton::clicked, this, [this]() { openLiveRoom(); });

        QHBoxLayout* titleLayout = new QHBoxLayout();
        QLineEdit* titleInput = new QLineEdit(config.title.c_str(), dialog);
        QPushButton* confirmTitle = new QPushButton("确认", dialog);
        titleLayout->addWidget(new QLabel("直播间标题:", dialog));
        titleLayout->addWidget(titleInput);
        titleLayout->addWidget(confirmTitle);
        layout->addLayout(titleLayout);

        QHBoxLayout* partitionLayout = new QHBoxLayout();
        QComboBox* partCombo = new QComboBox(dialog);
        QComboBox* areaCombo = new QComboBox(dialog);
        QPushButton* confirmPartition = new QPushButton("确认", dialog);
        partitionLayout->addWidget(new QLabel("直播间分区:", dialog));
        partitionLayout->addWidget(partCombo);
        partitionLayout->addWidget(areaCombo);
        partitionLayout->addWidget(confirmPartition);
        layout->addLayout(partitionLayout);

        struct Part { int id; QString name; std::vector<json11::Json> list; };
        std::vector<Part> parts;
        auto partition_data = Bili::BiliApi::getPartitionList(message);
        if (partition_data.is_array()) {
            for (const auto& item : partition_data.array_items()) {
                parts.push_back({item["id"].int_value(), QString::fromUtf8(item["name"].string_value().c_str()), item["list"].array_items()});
            }
        }
	    if (parts.empty()) {
		    showMessageDialog(QString::fromUtf8(message), "消息");
	    }

        size_t selected_part_index = 0;
        for (size_t i = 0; i < parts.size(); ++i) {
            partCombo->addItem(parts[i].name, parts[i].id);
            if (parts[i].id == config.part_id) selected_part_index = i;
        }
        partCombo->setCurrentIndex(static_cast<int>(selected_part_index));

        auto updateAreaCombo = [=](int part_index) {
            areaCombo->clear();
            if (part_index >= 0 && static_cast<size_t>(part_index) < parts.size() && !parts[part_index].list.empty()) {
                size_t selected_area_index = 0;
                for (size_t i = 0; i < parts[part_index].list.size(); ++i) {
                    int id = std::stoi(parts[part_index].list[i]["id"].string_value());
                    QString name = QString::fromUtf8(parts[part_index].list[i]["name"].string_value().c_str());
                    areaCombo->addItem(name, id);
                    if (id == config.area_id) selected_area_index = i;
                }
                areaCombo->setCurrentIndex(static_cast<int>(selected_area_index));
            } else {
                areaCombo->addItem("英雄联盟", 86);
                if (config.area_id == 86) areaCombo->setCurrentIndex(0);
            }
        };
        updateAreaCombo(static_cast<int>(selected_part_index));
        connect(partCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) { updateAreaCombo(index); });

        connect(confirmTitle, &QPushButton::clicked, [=]() {
            std::string new_title = titleInput->text().trimmed().toUtf8().constData();
		    std::string update_message;
            if (!new_title.empty() && Bili::BiliApi::updateRoomInfo(config, new_title, update_message)) {
                config.title = new_title;
                saveConfig();
                showMessageDialog("直播间标题已更新", "消息");
            } else {
		        showMessageDialog(QStringLiteral("直播间标题更新失败\n") + QString::fromUtf8(update_message.c_str()), "消息");
            }
        });

        connect(confirmPartition, &QPushButton::clicked, [=]() {
            config.part_id = partCombo->currentData().toInt();
            config.area_id = areaCombo->currentData().toInt();
            saveConfig();
            showMessageDialog("直播间分区已更新", "消息");
        });

        connect(dialog, &QDialog::finished, [=]() { dialog->deleteLater(); });
        dialog->exec();
    }

    void showMessageDialog(const QString& message, const QString& title) {
        QDialog* dialog = new QDialog((QWidget*)obs_frontend_get_main_window());
        dialog->setWindowTitle(title);
        QVBoxLayout* layout = new QVBoxLayout(dialog);
        layout->addWidget(new QLabel(message, dialog));
        QPushButton* confirm = new QPushButton("确认", dialog);
        layout->addWidget(confirm);
        connect(confirm, &QPushButton::clicked, [=]() { dialog->accept(); });
        connect(dialog, &QDialog::finished, [=]() { dialog->deleteLater(); });
        dialog->exec();
    }
};

static BilibiliStreamPlugin* plugin = nullptr;

bool obs_module_load(void) {
    Bili::BiliApi::init();
    auto main_window = static_cast<QMainWindow*>(obs_frontend_get_main_window());
    if (!main_window) return false;
    plugin = new BilibiliStreamPlugin(main_window);
    obs_log(LOG_INFO, "插件加载成功");
    return true;
}

void obs_module_unload(void) {
    Bili::BiliApi::cleanup();
    delete plugin;
    plugin = nullptr;
    obs_log(LOG_INFO, "插件已卸载");
}

#include "plugin-main.moc"
