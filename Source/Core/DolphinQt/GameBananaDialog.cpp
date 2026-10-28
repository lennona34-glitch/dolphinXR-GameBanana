// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/GameBananaDialog.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <set>
#include <sstream>
#include <thread>
#include <utility>

#include <mz.h>
#include <mz_os.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/IOFile.h"
#include "Common/IniFile.h"
#include "Common/MinizipUtil.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"

#include "Core/ActionReplay.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"
#include "Core/System.h"

#include "DolphinQt/QtUtils/DolphinFileDialog.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"
#include "DolphinQt/Settings.h"

#include "UICommon/ResourcePack/Manager.h"
#include "UICommon/ResourcePack/Manifest.h"
#include "UICommon/ResourcePack/ResourcePack.h"

namespace
{
struct CuratedGameEntry
{
  int gb_game_id;
  const char* display_name;
  const char* game_id;
  const char* platform;
};

constexpr std::array<CuratedGameEntry, 37> s_curated_games = {{
    {0, "All GameCube & Wii Games", "", "GC/Wii"},
    {5678, "Super Smash Bros. Brawl (Wii)", "RSBE01", "Wii"},
    {5706, "Super Smash Bros. Melee (GC)", "GALE01", "GC"},
    {5896, "Mario Kart Wii (Wii)", "RMCE01", "Wii"},
    {6555, "Mario Kart: Double Dash!! (GC)", "GM4E01", "GC"},
    {5798, "Super Mario Sunshine (GC)", "GMSE01", "GC"},
    {5950, "Super Mario Galaxy (Wii)", "RMGE01", "Wii"},
    {6867, "Super Mario Galaxy 2 (Wii)", "SB4E01", "Wii"},
    {6247, "New Super Mario Bros. Wii (Wii)", "SMNE01", "Wii"},
    {6173, "The Legend of Zelda: The Wind Waker (GC)", "GZLE01", "GC"},
    {6167, "The Legend of Zelda: Twilight Princess (GC/Wii)", "GZ2E01", "GC/Wii"},
    {6305, "The Legend of Zelda: Skyward Sword (Wii)", "SOUE01", "Wii"},
    {5855, "Sonic Adventure 2: Battle (GC)", "GSNE8P", "GC"},
    {5892, "Sonic Adventure DX: Director's Cut (GC)", "GXSE8P", "GC"},
    {6061, "Sonic Heroes (GC)", "G9SE8P", "GC"},
    {8236, "Sonic Riders (GC)", "GXEE8P", "GC"},
    {6512, "Sonic Riders: Zero Gravity (Wii)", "SRZE8P", "Wii"},
    {9598, "Sonic Unleashed (Wii)", "RSVE8P", "Wii"},
    {6243, "Sonic Colors (Wii)", "SNCE8P", "Wii"},
    {6511, "Sonic and the Black Knight (Wii)", "RESE8P", "Wii"},
    {6218, "Shadow the Hedgehog (GC)", "GUPE8P", "GC"},
    {7801, "Metroid Prime (GC)", "GM8E01", "GC"},
    {7802, "Metroid Prime 2: Echoes (GC)", "G2ME01", "GC"},
    {23926, "Metroid Prime 3: Corruption (Wii)", "RM3E01", "Wii"},
    {7030, "Paper Mario: The Thousand-Year Door (GC)", "G8ME01", "GC"},
    {6576, "Super Paper Mario (Wii)", "R8PE01", "Wii"},
    {6533, "Animal Crossing (GC)", "GAFE01", "GC"},
    {7120, "Luigi's Mansion (GC)", "GLME01", "GC"},
    {5776, "Kirby Air Ride (GC)", "GKYE01", "GC"},
    {5899, "F-Zero GX (GC)", "GFZE01", "GC"},
    {7122, "Pikmin (GC)", "GPIE01", "GC"},
    {5788, "Pikmin 2 (GC)", "GPVE01", "GC"},
    {17019, "Pokemon Colosseum (GC)", "GC6E01", "GC"},
    {8519, "Pokemon XD: Gale of Darkness (GC)", "GXXE01", "GC"},
    {6291, "Mario Party 6 (GC)", "GP6E01", "GC"},
    {6440, "Wario World (GC)", "GWWE01", "GC"},
    {854, "Resident Evil 4 (GC/Wii)", "G4BE08", "GC/Wii"},
}};

struct BuiltinCheatTemplate
{
  const char* name;
  const char* game_id;
  const char* game_name;
  const char* code_type;
  const char* creator;
  const char* description;
  const char* code_lines;
};

constexpr std::array<BuiltinCheatTemplate, 12> s_cheat_templates = {{
    {"Unlock All Characters & Stages", "GALE01", "Super Smash Bros. Melee", "Gecko", "Melee Community",
     "Unlocks all 25 characters, all stages, and sound test immediately.",
     "0445BF28 FFFFFFFF\n0445BF2C FFFFFFFF\n0445BF30 FFFFFFFF"},
    {"Widescreen 16:9 Aspect Ratio Override", "GALE01", "Super Smash Bros. Melee", "Gecko", "Dan Salvato / Achilles",
     "True 16:9 widescreen camera frustum adjustment for Melee (great for VR & modern displays).",
     "043B97E8 3FE38E39"},
    {"60 FPS Gameplay Mode", "GMSE01", "Super Mario Sunshine", "Gecko", "masterotaku / Pahaze",
     "Doubles Super Mario Sunshine framerate from 30 FPS to 60 FPS.",
     "042F9844 60000000\n044167B8 3F800000\n0240D00A 00000001"},
    {"Widescreen 16:9 HUD & Frustum Fix", "GMSE01", "Super Mario Sunshine", "Gecko", "Gamemasterplc",
     "Fixes culling and 2D HUD stretching in 16:9 widescreen / VR.",
     "040290C4 38600001\n04414900 3FA28F5C"},
    {"Unlock Everything (All Karts, Bikes, Courses & Licenses)", "RMCE01", "Mario Kart Wii", "Gecko", "mdmwii",
     "Unlocks all characters, vehicles, Mirror mode, and cups on the active license.",
     "089BD8E8 FFFFFFFF\n20040004 00000000"},
    {"60 FPS Split-Screen / Multiplayer", "RMCE01", "Mario Kart Wii", "Gecko", "Vega",
     "Forces 60 FPS even in 3-player and 4-player modes.",
     "04554E64 38000001"},
    {"Unlock Everything (All Karts, Cups & Mirror Mode)", "GM4E01", "Mario Kart: Double Dash!!", "Gecko", "Ralf",
     "Unlocks all secret karts, parade kart, Special Cup, All-Cup Tour, and Mirror Mode.",
     "043B06B8 FFFFFFFF"},
    {"Fast Sail / Swift Sail Speed", "GZLE01", "The Legend of Zelda: The Wind Waker", "Gecko", "Zelda Modding",
     "Doubles boat sailing speed and keeps wind direction favorable.",
     "043FE4D8 40200000"},
    {"No HUD / Clean VR View Toggle", "GZ2E01", "The Legend of Zelda: Twilight Princess", "Gecko", "VR Community",
     "Reduces on-screen HUD opacity for an unobstructed VR view.",
     "04431B20 00000000"},
    {"Max Rings (999 Rings in Level)", "GSNE8P", "Sonic Adventure 2: Battle", "Gecko", "SA2 Community",
     "Sets current ring count to 999 for Sonic/Shadow stages.",
     "021E0C42 000003E7"},
    {"Infinite Star Power & BP (99 BP)", "G8ME01", "Paper Mario: The Thousand-Year Door", "Gecko", "TTYD Modding",
     "Sets Badge Points to 99 so any badge combination can be equipped.",
     "023B0F9A 00000063"},
    {"Custom 32-Bit RAM Write Template", "RSBE01", "Super Smash Bros. Brawl", "Gecko", "DolphinXR",
     "Template for a standard Gecko 04-type 32-bit RAM write.",
     "04800000 00000001"},
}};

std::string SanitizeFolderName(const std::string& raw)
{
  std::string out;
  out.reserve(raw.size());
  for (const char c : raw)
  {
    const unsigned char uc = static_cast<unsigned char>(c);
    if (std::isalnum(uc) || c == '-' || c == '_' || c == '.' || c == ' ')
      out.push_back(c);
    else
      out.push_back('_');
  }
  while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
    out.pop_back();
  if (out.empty())
    out = "GameBanana_Mod";
  return out;
}

QString CleanHtmlDescription(const QString& html)
{
  if (html.isEmpty())
    return QString();

  QString text = html;
  text.replace(QRegularExpression(QStringLiteral("<br\\s*/?>"),
                                  QRegularExpression::CaseInsensitiveOption),
               QStringLiteral("\n"));
  text.replace(QRegularExpression(QStringLiteral("</p>|</li>|</div>|</h[1-6]>"),
                                  QRegularExpression::CaseInsensitiveOption),
               QStringLiteral("\n"));
  text.replace(QRegularExpression(QStringLiteral("<li[^>]*>"),
                                  QRegularExpression::CaseInsensitiveOption),
               QStringLiteral("  * "));
  text.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
  text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
  text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
  text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
  text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
  text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
  text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
  text.replace(QStringLiteral("&#039;"), QStringLiteral("'"));
  return text.trimmed();
}

QString FormatBytes(s64 bytes)
{
  if (bytes <= 0)
    return QStringLiteral("Unknown size");
  const double kb = static_cast<double>(bytes) / 1024.0;
  if (kb < 1024.0)
    return QStringLiteral("%1 KB").arg(kb, 0, 'f', 1);
  const double mb = kb / 1024.0;
  if (mb < 1024.0)
    return QStringLiteral("%1 MB").arg(mb, 0, 'f', 2);
  const double gb = mb / 1024.0;
  return QStringLiteral("%1 GB").arg(gb, 0, 'f', 2);
}

bool LooksLikeGameIDFolder(const std::string& folder)
{
  if (folder.size() != 3 && folder.size() != 6)
    return false;
  return std::ranges::all_of(folder, [](char c) {
    const unsigned char uc = static_cast<unsigned char>(c);
    return std::isupper(uc) || std::isdigit(uc);
  });
}

std::vector<u8> FetchUrlBytes(Common::HttpRequest& http, const std::string& url,
                              const Common::HttpRequest::Headers& headers = {
                                  {"User-Agent", "DolphinXR-GameBanana/0.4"},
                                  {"Accept", "application/json"}})
{
  const auto resp = http.Get(url, headers, Common::HttpRequest::AllowedReturnCodes::All);
  if (resp && !resp->empty() && http.GetLastResponseCode() >= 200)
    return *resp;

  // Fallback to system curl.exe if libcurl/Schannel fails due to local TLS/CRL/proxy settings
  QProcess proc;
  QStringList args;
  args << QStringLiteral("-s") << QStringLiteral("-L") << QStringLiteral("-k")
       << QStringLiteral("--max-time") << QStringLiteral("25") << QStringLiteral("-A")
       << QStringLiteral("DolphinXR-GameBanana/0.4") << QString::fromStdString(url);
  proc.start(QStringLiteral("curl.exe"), args);
  if (proc.waitForFinished(28000) && proc.exitStatus() == QProcess::NormalExit &&
      proc.exitCode() == 0)
  {
    const QByteArray out = proc.readAllStandardOutput();
    if (!out.isEmpty())
    {
      return std::vector<u8>(reinterpret_cast<const u8*>(out.constData()),
                             reinterpret_cast<const u8*>(out.constData()) + out.size());
    }
  }

  if (resp && !resp->empty())
    return *resp;
  return {};
}
}  // namespace

GameBananaDialog::GameBananaDialog(QWidget* parent)
    : QDialog(parent), m_alive(std::make_shared<std::atomic<bool>>(true))
{
  setWindowTitle(tr("GameBanana Mod & Cheat Studio - DolphinXR"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  resize(1120, 740);

  CreateWidgets();
  ConnectWidgets();

  PopulateCheatTemplates();
  RefreshInstalledMods();
  RefreshExistingCodes();
}

GameBananaDialog::~GameBananaDialog()
{
  if (m_alive)
    m_alive->store(false, std::memory_order_release);
}

void GameBananaDialog::showEvent(QShowEvent* event)
{
  QDialog::showEvent(event);
  if (m_request_serial.load() == 0)
    TriggerSearch(1);
}

void GameBananaDialog::SelectGameByGameID(const std::string& game_id, const std::string& game_title)
{
  if (game_id.empty() && game_title.empty())
    return;

  if (!game_id.empty())
  {
    m_cheat_game_id_edit->setText(QString::fromStdString(game_id));
    RefreshExistingCodes();
  }

  std::string prefix3 = game_id.substr(0, std::min<size_t>(3, game_id.size()));
  // Map Wii versions of cross-platform titles to their shared GameBanana hub
  if (prefix3 == "RZD")
    prefix3 = "GZ2";  // Twilight Princess (Wii -> GC/Wii hub)
  else if (prefix3 == "RB4")
    prefix3 = "G4B";  // Resident Evil 4 (Wii -> GC/Wii hub)

  int matched_combo_index = -1;
  if (!game_id.empty())
  {
    for (int i = 1; i < m_game_combo->count(); ++i)
    {
      const QString item_game_id = m_game_combo->itemData(i, Qt::UserRole + 1).toString();
      if (item_game_id.compare(QString::fromStdString(game_id), Qt::CaseInsensitive) == 0 ||
          (!prefix3.empty() &&
           item_game_id.startsWith(QString::fromStdString(prefix3), Qt::CaseInsensitive)))
      {
        matched_combo_index = i;
        break;
      }
    }
  }

  if (matched_combo_index < 0 && !game_title.empty())
  {
    const QString qtitle = QString::fromStdString(game_title).trimmed();
    for (int i = 1; i < m_game_combo->count(); ++i)
    {
      QString item_text = m_game_combo->itemText(i);
      const qsizetype paren_pos = item_text.indexOf(QLatin1Char('('));
      if (paren_pos > 0)
        item_text = item_text.left(paren_pos).trimmed();
      if (!item_text.isEmpty() &&
          (item_text.compare(qtitle, Qt::CaseInsensitive) == 0 ||
           qtitle.contains(item_text, Qt::CaseInsensitive) ||
           item_text.contains(qtitle, Qt::CaseInsensitive)))
      {
        matched_combo_index = i;
        break;
      }
    }
  }

  // If the game is not in the curated list, try resolving its GameBanana hub ID via NameMatch
  if (matched_combo_index < 0 && !game_title.empty())
  {
    Common::HttpRequest http(std::chrono::milliseconds{4000});
    http.FollowRedirects(5);
    const std::string match_url =
        "https://gamebanana.com/apiv11/Util/Game/NameMatch?_sName=" +
        http.EscapeComponent(game_title);
    const std::vector<u8> bytes = FetchUrlBytes(http, match_url);
    if (!bytes.empty())
    {
      const QJsonDocument doc = QJsonDocument::fromJson(
          QByteArray(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<qsizetype>(bytes.size())));
      if (doc.isArray() && !doc.array().isEmpty())
      {
        const QJsonObject first_game = doc.array().at(0).toObject();
        const int resolved_gb_id = first_game.value(QStringLiteral("_idRow")).toInt();
        const QString resolved_name = first_game.value(QStringLiteral("_sName")).toString();
        if (resolved_gb_id > 0 && !resolved_name.isEmpty())
        {
          const QSignalBlocker blocker(m_game_combo);
          m_game_combo->addItem(resolved_name, resolved_gb_id);
          matched_combo_index = m_game_combo->count() - 1;
          m_game_combo->setItemData(matched_combo_index, QString::fromStdString(game_id),
                                    Qt::UserRole + 1);
        }
      }
    }
  }

  const QSignalBlocker combo_blocker(m_game_combo);
  const QSignalBlocker search_blocker(m_search_edit);
  if (matched_combo_index >= 0)
  {
    m_game_combo->setCurrentIndex(matched_combo_index);
    m_search_edit->clear();
    TriggerSearch(1);
  }
  else if (!game_title.empty())
  {
    m_game_combo->setCurrentIndex(0);
    m_search_edit->setText(QString::fromStdString(game_title));
    TriggerSearch(1);
  }
}

void GameBananaDialog::CreateWidgets()
{
  auto* main_layout = new QVBoxLayout(this);

  auto* header_label = new QLabel(
      tr("<b>GameBanana Mod, Texture Pack & Cheat Manager</b> &mdash; Browse, download, and "
         "install GameCube &amp; Wii mods, HD texture packs, resource packs, Riivolution patches, "
         "and Gecko/AR codes directly into DolphinXR."));
  header_label->setWordWrap(true);
  main_layout->addWidget(header_label);

  m_tabs = new QTabWidget(this);
  m_tabs->addTab(CreateBrowseTab(), tr("Browse && Download Mods (GameBanana)"));
  m_tabs->addTab(CreateInstalledTab(), tr("Installed Mods && Texture Packs"));
  m_tabs->addTab(CreateCheatsTab(), tr("Cheats && Gecko / AR Studio"));
  main_layout->addWidget(m_tabs, 1);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  main_layout->addWidget(buttons);
}

QWidget* GameBananaDialog::CreateBrowseTab()
{
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  auto* filter_row = new QHBoxLayout();
  filter_row->addWidget(new QLabel(tr("Game:")));
  m_game_combo = new QComboBox(page);
  for (const auto& g : s_curated_games)
  {
    m_game_combo->addItem(QString::fromUtf8(g.display_name), g.gb_game_id);
    const int idx = m_game_combo->count() - 1;
    m_game_combo->setItemData(idx, QString::fromLatin1(g.game_id), Qt::UserRole + 1);
  }
  m_game_combo->setMinimumWidth(240);
  filter_row->addWidget(m_game_combo);

  filter_row->addWidget(new QLabel(tr("Category:")));
  m_category_combo = new QComboBox(page);
  m_category_combo->addItem(tr("All Categories"), QStringLiteral(""));
  m_category_combo->addItem(tr("Skins / Costumes / Textures"), QStringLiteral("skin"));
  m_category_combo->addItem(tr("Stages / Courses / Maps"), QStringLiteral("stage"));
  m_category_combo->addItem(tr("Gameplay / Overhauls / Riivolution"), QStringLiteral("mod"));
  m_category_combo->addItem(tr("UI / HUD / Menus"), QStringLiteral("ui"));
  m_category_combo->addItem(tr("Audio / Music / Voice"), QStringLiteral("sound"));
  m_category_combo->addItem(tr("Graphics / HD / VR / Shaders"), QStringLiteral("hd"));
  m_category_combo->addItem(tr("Codes / Gecko / Cheats"), QStringLiteral("code"));
  filter_row->addWidget(m_category_combo);

  filter_row->addWidget(new QLabel(tr("Sort:")));
  m_sort_combo = new QComboBox(page);
  m_sort_combo->addItem(tr("Most Popular / Default"), QStringLiteral(""));
  m_sort_combo->addItem(tr("Most Liked"), QStringLiteral("Generic_MostLiked"));
  m_sort_combo->addItem(tr("Most Downloaded"), QStringLiteral("Generic_MostDownloaded"));
  m_sort_combo->addItem(tr("Most Viewed"), QStringLiteral("Generic_MostViewed"));
  m_sort_combo->addItem(tr("Newest Submissions"), QStringLiteral("Generic_Newest"));
  m_sort_combo->addItem(tr("Recently Updated"), QStringLiteral("Generic_LatestModified"));
  filter_row->addWidget(m_sort_combo);
  filter_row->addStretch();

  layout->addLayout(filter_row);

  auto* search_row = new QHBoxLayout();
  search_row->addWidget(new QLabel(tr("Search:")));
  m_search_edit = new QLineEdit(page);
  m_search_edit->setMinimumWidth(260);
  m_search_edit->setPlaceholderText(tr("Search GameBanana mods, HD textures, VR fixes..."));
  search_row->addWidget(m_search_edit, 1);

  m_search_btn = new NonDefaultQPushButton(tr("Search"), page);
  search_row->addWidget(m_search_btn);

  m_prev_page_btn = new NonDefaultQPushButton(tr("< Prev"), page);
  m_prev_page_btn->setEnabled(false);
  search_row->addWidget(m_prev_page_btn);

  m_page_label = new QLabel(tr("Page 1"), page);
  search_row->addWidget(m_page_label);

  m_next_page_btn = new NonDefaultQPushButton(tr("Next >"), page);
  search_row->addWidget(m_next_page_btn);

  layout->addLayout(search_row);

  auto* splitter = new QSplitter(Qt::Horizontal, page);

  // Left: Mods Table
  auto* left_widget = new QWidget(splitter);
  auto* left_layout = new QVBoxLayout(left_widget);
  left_layout->setContentsMargins(0, 0, 0, 0);

  m_mods_table = new QTableWidget(0, 6, left_widget);
  m_mods_table->setHorizontalHeaderLabels(
      {tr("Mod Name"), tr("Category"), tr("Game"), tr("Author"), tr("Likes"), tr("Views")});
  m_mods_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_mods_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_mods_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_mods_table->setAlternatingRowColors(true);
  m_mods_table->verticalHeader()->hide();
  m_mods_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_mods_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_mods_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  m_mods_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  m_mods_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
  m_mods_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
  left_layout->addWidget(m_mods_table, 1);

  m_status_label = new QLabel(tr("Ready to browse GameBanana."), left_widget);
  left_layout->addWidget(m_status_label);

  // Right: Mod Inspector
  auto* right_group = new QGroupBox(tr("Mod Inspector && 1-Click Installer"), splitter);
  auto* right_layout = new QVBoxLayout(right_group);

  m_preview_image_label = new QLabel(tr("Select a mod to preview"), right_group);
  m_preview_image_label->setMinimumSize(340, 185);
  m_preview_image_label->setMaximumHeight(210);
  m_preview_image_label->setAlignment(Qt::AlignCenter);
  m_preview_image_label->setStyleSheet(
      QStringLiteral("QLabel { background-color: #1a1d24; color: #9aa0a6; "
                     "border: 1px solid #353a45; border-radius: 4px; }"));
  right_layout->addWidget(m_preview_image_label);

  m_detail_title = new QLabel(tr("No Mod Selected"), right_group);
  QFont title_font = m_detail_title->font();
  title_font.setPointSize(title_font.pointSize() + 2);
  title_font.setBold(true);
  m_detail_title->setFont(title_font);
  m_detail_title->setWordWrap(true);
  right_layout->addWidget(m_detail_title);

  m_detail_meta = new QLabel(tr("Choose a submission from the table on the left."), right_group);
  m_detail_meta->setWordWrap(true);
  right_layout->addWidget(m_detail_meta);

  m_detail_stats = new QLabel(QString(), right_group);
  m_detail_stats->setWordWrap(true);
  right_layout->addWidget(m_detail_stats);

  m_detail_desc = new QTextBrowser(right_group);
  m_detail_desc->setOpenExternalLinks(true);
  m_detail_desc->setPlaceholderText(tr("Mod description and installation notes will appear here."));
  right_layout->addWidget(m_detail_desc, 1);

  right_layout->addWidget(new QLabel(tr("Select Archive / File to Download:"), right_group));
  m_files_combo = new QComboBox(right_group);
  right_layout->addWidget(m_files_combo);

  m_download_progress = new QProgressBar(right_group);
  m_download_progress->setRange(0, 100);
  m_download_progress->setValue(0);
  right_layout->addWidget(m_download_progress);

  auto* action_row = new QHBoxLayout();
  m_install_btn = new NonDefaultQPushButton(tr("Download && Install Mod"), right_group);
  m_install_btn->setEnabled(false);
  m_open_browser_btn = new NonDefaultQPushButton(tr("Open on GameBanana"), right_group);
  m_open_browser_btn->setEnabled(false);
  action_row->addWidget(m_install_btn, 1);
  action_row->addWidget(m_open_browser_btn);
  right_layout->addLayout(action_row);

  splitter->addWidget(left_widget);
  splitter->addWidget(right_group);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);

  layout->addWidget(splitter, 1);
  return page;
}

QWidget* GameBananaDialog::CreateInstalledTab()
{
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  auto* toggles_group = new QGroupBox(tr("Dolphin Graphics & Texture Pack Settings"), page);
  auto* toggles_layout = new QHBoxLayout(toggles_group);

  m_chk_hires_textures = new QCheckBox(tr("Load Custom Textures (GFX_HIRES_TEXTURES)"), page);
  m_chk_hires_textures->setChecked(Config::Get(Config::GFX_HIRES_TEXTURES));
  toggles_layout->addWidget(m_chk_hires_textures);

  m_chk_cache_textures =
      new QCheckBox(tr("Prefetch Custom Textures (GFX_CACHE_HIRES_TEXTURES)"), page);
  m_chk_cache_textures->setChecked(Config::Get(Config::GFX_CACHE_HIRES_TEXTURES));
  toggles_layout->addWidget(m_chk_cache_textures);

  m_chk_graphics_mods = new QCheckBox(tr("Enable Graphics Mods (GFX_MODS_ENABLE)"), page);
  m_chk_graphics_mods->setChecked(Config::Get(Config::GFX_MODS_ENABLE));
  toggles_layout->addWidget(m_chk_graphics_mods);
  toggles_layout->addStretch();

  layout->addWidget(toggles_group);

  m_installed_table = new QTableWidget(0, 5, page);
  m_installed_table->setHorizontalHeaderLabels(
      {tr("Status"), tr("Mod / Pack Name"), tr("Type"), tr("Game ID"), tr("Location")});
  m_installed_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_installed_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_installed_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_installed_table->setAlternatingRowColors(true);
  m_installed_table->verticalHeader()->hide();
  m_installed_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  m_installed_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_installed_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  m_installed_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  m_installed_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
  layout->addWidget(m_installed_table, 1);

  auto* btn_row = new QHBoxLayout();
  m_btn_refresh_installed = new NonDefaultQPushButton(tr("Refresh List"), page);
  m_btn_toggle_installed = new NonDefaultQPushButton(tr("Toggle Enable / Disable"), page);
  m_btn_import_zip = new NonDefaultQPushButton(tr("Import Local Mod (.zip)..."), page);
  m_btn_open_mod_dir = new NonDefaultQPushButton(tr("Open Selected Folder"), page);
  m_btn_open_textures_dir = new NonDefaultQPushButton(tr("Open Load/Textures Folder"), page);
  m_btn_delete_mod = new NonDefaultQPushButton(tr("Uninstall / Delete Mod"), page);

  btn_row->addWidget(m_btn_refresh_installed);
  btn_row->addWidget(m_btn_toggle_installed);
  btn_row->addWidget(m_btn_import_zip);
  btn_row->addWidget(m_btn_open_mod_dir);
  btn_row->addWidget(m_btn_open_textures_dir);
  btn_row->addStretch();
  btn_row->addWidget(m_btn_delete_mod);

  layout->addLayout(btn_row);
  return page;
}

QWidget* GameBananaDialog::CreateCheatsTab()
{
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  auto* top_bar = new QHBoxLayout();
  top_bar->addWidget(new QLabel(tr("Preset Game:"), page));
  m_cheat_game_combo = new QComboBox(page);
  for (const auto& g : s_curated_games)
  {
    if (std::strlen(g.game_id) == 0)
      continue;
    m_cheat_game_combo->addItem(
        QStringLiteral("%1 [%2]").arg(QString::fromUtf8(g.display_name),
                                      QString::fromLatin1(g.game_id)),
        QString::fromLatin1(g.game_id));
  }
  top_bar->addWidget(m_cheat_game_combo);

  top_bar->addWidget(new QLabel(tr("Active Game ID (6 chars):"), page));
  m_cheat_game_id_edit = new QLineEdit(QStringLiteral("GALE01"), page);
  m_cheat_game_id_edit->setMaximumWidth(100);
  top_bar->addWidget(m_cheat_game_id_edit);

  m_chk_enable_cheats = new QCheckBox(tr("Enable Cheats Globally (MAIN_ENABLE_CHEATS)"), page);
  m_chk_enable_cheats->setChecked(Config::Get(Config::MAIN_ENABLE_CHEATS));
  top_bar->addWidget(m_chk_enable_cheats);
  top_bar->addStretch();

  layout->addLayout(top_bar);

  auto* splitter = new QSplitter(Qt::Horizontal, page);

  // Left side: Built-in Cheat Templates + Existing Game INI Codes
  auto* left_widget = new QWidget(splitter);
  auto* left_layout = new QVBoxLayout(left_widget);
  left_layout->setContentsMargins(0, 0, 0, 0);

  auto* tpl_group = new QGroupBox(tr("Curated GameCube / Wii / VR Enhancement & Cheat Templates"),
                                  left_widget);
  auto* tpl_layout = new QVBoxLayout(tpl_group);
  m_templates_table = new QTableWidget(0, 4, tpl_group);
  m_templates_table->setHorizontalHeaderLabels(
      {tr("Cheat / Patch Name"), tr("Game"), tr("Game ID"), tr("Type")});
  m_templates_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_templates_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_templates_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_templates_table->setAlternatingRowColors(true);
  m_templates_table->verticalHeader()->hide();
  m_templates_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_templates_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_templates_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  m_templates_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  tpl_layout->addWidget(m_templates_table, 1);

  m_btn_use_template =
      new NonDefaultQPushButton(tr("Load Selected Template into Editor ->"), tpl_group);
  tpl_layout->addWidget(m_btn_use_template);
  left_layout->addWidget(tpl_group, 3);

  auto* existing_group =
      new QGroupBox(tr("Installed Codes in GameSettings/<GameID>.ini"), left_widget);
  auto* existing_layout = new QVBoxLayout(existing_group);
  m_existing_codes_list = new QListWidget(existing_group);
  existing_layout->addWidget(m_existing_codes_list, 1);
  m_btn_save_toggled_codes =
      new NonDefaultQPushButton(tr("Save Checked States to Game INI"), existing_group);
  existing_layout->addWidget(m_btn_save_toggled_codes);
  left_layout->addWidget(existing_group, 2);

  // Right side: Gecko / AR Code Studio & Quick Memory Patch Builder
  auto* right_group = new QGroupBox(tr("Gecko && Action Replay Code Studio"), splitter);
  auto* right_layout = new QVBoxLayout(right_group);

  auto* form = new QFormLayout();
  m_cheat_name_edit = new QLineEdit(right_group);
  m_cheat_name_edit->setPlaceholderText(tr("e.g., 60 FPS / Widescreen Fix / Infinite Health"));
  form->addRow(tr("Code Name:"), m_cheat_name_edit);

  m_cheat_creator_edit = new QLineEdit(QStringLiteral("GameBanana / DolphinXR"), right_group);
  form->addRow(tr("Creator:"), m_cheat_creator_edit);

  m_cheat_type_combo = new QComboBox(right_group);
  m_cheat_type_combo->addItem(tr("Gecko Code ([Gecko])"), QStringLiteral("Gecko"));
  m_cheat_type_combo->addItem(tr("Action Replay Code ([ActionReplay])"), QStringLiteral("AR"));
  form->addRow(tr("Code Format:"), m_cheat_type_combo);

  m_cheat_notes_edit = new QLineEdit(right_group);
  m_cheat_notes_edit->setPlaceholderText(tr("Optional description saved to GameSettings INI"));
  form->addRow(tr("Notes:"), m_cheat_notes_edit);
  right_layout->addLayout(form);

  auto* builder_group = new QGroupBox(tr("Quick Gecko RAM Write Line Builder"), right_group);
  auto* builder_layout = new QHBoxLayout(builder_group);
  m_patch_addr_edit = new QLineEdit(QStringLiteral("80000000"), builder_group);
  m_patch_addr_edit->setPlaceholderText(tr("RAM Address (Hex)"));
  m_patch_val_edit = new QLineEdit(QStringLiteral("00000001"), builder_group);
  m_patch_val_edit->setPlaceholderText(tr("Value (Hex)"));
  m_patch_type_combo = new QComboBox(builder_group);
  m_patch_type_combo->addItem(tr("32-bit Write (04)"), QStringLiteral("04"));
  m_patch_type_combo->addItem(tr("16-bit Write (02)"), QStringLiteral("02"));
  m_patch_type_combo->addItem(tr("8-bit Write (00)"), QStringLiteral("00"));
  m_btn_append_patch = new NonDefaultQPushButton(tr("Append Line"), builder_group);

  builder_layout->addWidget(new QLabel(tr("Addr:"), builder_group));
  builder_layout->addWidget(m_patch_addr_edit);
  builder_layout->addWidget(new QLabel(tr("Val:"), builder_group));
  builder_layout->addWidget(m_patch_val_edit);
  builder_layout->addWidget(m_patch_type_combo);
  builder_layout->addWidget(m_btn_append_patch);
  right_layout->addWidget(builder_group);

  right_layout->addWidget(new QLabel(tr("Code Lines (XXXXXXXX YYYYYYYY):"), right_group));
  m_cheat_lines_edit = new QPlainTextEdit(right_group);
  QFont mono(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
  m_cheat_lines_edit->setFont(mono);
  m_cheat_lines_edit->setPlaceholderText(QStringLiteral("043B97E8 3FE38E39\n0445BF28 FFFFFFFF"));
  right_layout->addWidget(m_cheat_lines_edit, 1);

  auto* save_row = new QHBoxLayout();
  m_btn_save_cheat =
      new NonDefaultQPushButton(tr("Save && Enable Code in Game INI"), right_group);
  m_btn_open_ini = new NonDefaultQPushButton(tr("Open Game INI File"), right_group);
  save_row->addWidget(m_btn_save_cheat, 1);
  save_row->addWidget(m_btn_open_ini);
  right_layout->addLayout(save_row);

  splitter->addWidget(left_widget);
  splitter->addWidget(right_group);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);

  layout->addWidget(splitter, 1);
  return page;
}

void GameBananaDialog::ConnectWidgets()
{
  // Browse Tab
  connect(m_search_btn, &QPushButton::clicked, this, [this] { TriggerSearch(1); });
  connect(m_search_edit, &QLineEdit::returnPressed, this, [this] { TriggerSearch(1); });
  connect(m_game_combo, &QComboBox::currentIndexChanged, this, [this](int) { TriggerSearch(1); });
  connect(m_category_combo, &QComboBox::currentIndexChanged, this,
          [this](int) { TriggerSearch(1); });
  connect(m_sort_combo, &QComboBox::currentIndexChanged, this, [this](int) { TriggerSearch(1); });
  connect(m_prev_page_btn, &QPushButton::clicked, this, [this] {
    if (m_current_page > 1)
      TriggerSearch(m_current_page - 1);
  });
  connect(m_next_page_btn, &QPushButton::clicked, this,
          [this] { TriggerSearch(m_current_page + 1); });

  connect(m_mods_table, &QTableWidget::itemSelectionChanged, this,
          &GameBananaDialog::OnModSelected);
  connect(m_install_btn, &QPushButton::clicked, this, &GameBananaDialog::OnDownloadAndInstall);
  connect(m_open_browser_btn, &QPushButton::clicked, this, &GameBananaDialog::OnOpenInBrowser);

  // Installed Tab
  connect(m_chk_hires_textures, &QCheckBox::toggled, this, [](bool checked) {
    Config::SetBaseOrCurrent(Config::GFX_HIRES_TEXTURES, checked);
    Config::Save();
  });
  connect(m_chk_cache_textures, &QCheckBox::toggled, this, [](bool checked) {
    Config::SetBaseOrCurrent(Config::GFX_CACHE_HIRES_TEXTURES, checked);
    Config::Save();
  });
  connect(m_chk_graphics_mods, &QCheckBox::toggled, this, [](bool checked) {
    Config::SetBaseOrCurrent(Config::GFX_MODS_ENABLE, checked);
    Config::Save();
  });

  connect(m_btn_refresh_installed, &QPushButton::clicked, this,
          &GameBananaDialog::RefreshInstalledMods);
  connect(m_btn_toggle_installed, &QPushButton::clicked, this,
          &GameBananaDialog::OnToggleInstalledMod);
  connect(m_btn_import_zip, &QPushButton::clicked, this, &GameBananaDialog::OnImportLocalMod);
  connect(m_btn_open_mod_dir, &QPushButton::clicked, this,
          &GameBananaDialog::OnOpenSelectedModFolder);
  connect(m_btn_open_textures_dir, &QPushButton::clicked, this,
          &GameBananaDialog::OnOpenTexturesFolder);
  connect(m_btn_delete_mod, &QPushButton::clicked, this, &GameBananaDialog::OnDeleteInstalledMod);

  // Cheats Tab
  connect(m_cheat_game_combo, &QComboBox::currentIndexChanged, this, [this](int idx) {
    if (idx >= 0)
    {
      const QString gid = m_cheat_game_combo->itemData(idx).toString();
      if (!gid.isEmpty())
        m_cheat_game_id_edit->setText(gid);
    }
  });
  connect(m_cheat_game_id_edit, &QLineEdit::textChanged, this,
          [this](const QString&) { RefreshExistingCodes(); });
  connect(m_chk_enable_cheats, &QCheckBox::toggled, this, [](bool checked) {
    Config::SetBaseOrCurrent(Config::MAIN_ENABLE_CHEATS, checked);
    Config::Save();
  });
  connect(m_btn_use_template, &QPushButton::clicked, this, &GameBananaDialog::OnApplyTemplate);
  connect(m_templates_table, &QTableWidget::itemDoubleClicked, this,
          [this](QTableWidgetItem*) { OnApplyTemplate(); });
  connect(m_btn_save_toggled_codes, &QPushButton::clicked, this,
          &GameBananaDialog::OnSaveToggledExistingCodes);
  connect(m_btn_append_patch, &QPushButton::clicked, this, &GameBananaDialog::OnAppendQuickPatch);
  connect(m_btn_save_cheat, &QPushButton::clicked, this, &GameBananaDialog::OnSaveNewCheatCode);
  connect(m_btn_open_ini, &QPushButton::clicked, this, &GameBananaDialog::OnOpenGameIni);
}

std::string GameBananaDialog::GetSelectedTargetGameID() const
{
  const int idx = m_game_combo->currentIndex();
  if (idx >= 0)
  {
    const QString gid = m_game_combo->itemData(idx, Qt::UserRole + 1).toString();
    if (!gid.isEmpty())
      return gid.toStdString();
  }
  return {};
}

void GameBananaDialog::TriggerSearch(int page)
{
  m_current_page = std::max(1, page);
  m_page_label->setText(tr("Page %1").arg(m_current_page));
  m_prev_page_btn->setEnabled(m_current_page > 1);

  const int gb_game_id = m_game_combo->currentData().toInt();
  const std::string target_game_id = GetSelectedTargetGameID();
  const std::string search_term = m_search_edit->text().trimmed().toStdString();
  const std::string sort_mode = m_sort_combo->currentData().toString().toStdString();
  const std::string cat_filter = m_category_combo->currentData().toString().toStdString();

  const int req_id = ++m_request_serial;
  m_status_label->setText(tr("Fetching mods from GameBanana API..."));
  m_search_btn->setEnabled(false);

  std::weak_ptr<std::atomic<bool>> weak_alive = m_alive;
  QPointer<GameBananaDialog> self(this);

  std::thread([self, weak_alive, req_id, gb_game_id, target_game_id, search_term, sort_mode,
               cat_filter, page = m_current_page]() {
    auto alive = weak_alive.lock();
    if (!alive || !alive->load(std::memory_order_acquire))
      return;

    Common::HttpRequest http(std::chrono::milliseconds{15000},
                             [weak_alive](s64, s64, s64, s64) -> bool {
                               auto a = weak_alive.lock();
                               return a && a->load(std::memory_order_acquire);
                             });
    http.FollowRedirects(5);

    std::vector<int> games_to_fetch;
    if (gb_game_id > 0)
    {
      games_to_fetch.push_back(gb_game_id);
    }
    else if (search_term.size() >= 2)
    {
      // Global GameBanana Mod search when a search query is entered under "All GameCube & Wii Games"
      games_to_fetch.push_back(0);
    }
    else
    {
      // Verified top GameCube & Wii hubs on GameBanana (Brawl, Melee, MKWii, Sunshine, Wind Waker, SA2B)
      games_to_fetch = {5678, 5706, 5896, 5798, 6173, 5855};
    }

    std::vector<ModItem> parsed_mods;
    bool network_ok = false;

    for (const int gid : games_to_fetch)
    {
      if (!alive->load(std::memory_order_acquire))
        return;

      std::string url;
      if (search_term.size() >= 2)
      {
        url = "https://gamebanana.com/apiv11/Util/Search/Results?_nPage=" + std::to_string(page) +
              "&_nPerpage=25&_sModelName=Mod&_sOrder=best_match" +
              "&_sSearchString=" + http.EscapeComponent(search_term);
        if (gid > 0)
          url += "&_idGameRow=" + std::to_string(gid);
      }
      else
      {
        url = "https://gamebanana.com/apiv11/Mod/Index?_nPage=" + std::to_string(page) +
              "&_nPerpage=" + (games_to_fetch.size() == 1 ? "25" : "8") +
              "&_aFilters%5BGeneric_Game%5D=" + std::to_string(gid);
        if (!sort_mode.empty() && sort_mode != "default")
          url += "&_sSort=" + sort_mode;
      }

      const std::vector<u8> resp = FetchUrlBytes(http, url);
      if (resp.empty())
        continue;

      network_ok = true;
      const QByteArray bytes(reinterpret_cast<const char*>(resp.data()),
                             static_cast<qsizetype>(resp.size()));
      const QJsonDocument doc = QJsonDocument::fromJson(bytes);
      if (!doc.isObject())
        continue;

      const QJsonArray records = doc.object().value(QStringLiteral("_aRecords")).toArray();
      for (const QJsonValue& val : records)
      {
        if (!val.isObject())
          continue;
        const QJsonObject rec = val.toObject();

        ModItem item;
        item.id = rec.value(QStringLiteral("_idRow")).toInt();
        item.model_name =
            rec.value(QStringLiteral("_sModelName")).toString(QStringLiteral("Mod")).toStdString();
        if (item.id <= 0 || item.model_name != "Mod")
          continue;

        item.name = rec.value(QStringLiteral("_sName")).toString().toStdString();
        item.profile_url = rec.value(QStringLiteral("_sProfileUrl")).toString().toStdString();
        item.version = rec.value(QStringLiteral("_sVersion")).toString().toStdString();
        item.likes = rec.value(QStringLiteral("_nLikeCount")).toInt();
        item.views = rec.value(QStringLiteral("_nViewCount")).toInt();
        item.date_added = static_cast<s64>(rec.value(QStringLiteral("_tsDateAdded")).toDouble());
        item.date_updated =
            static_cast<s64>(rec.value(QStringLiteral("_tsDateModified")).toDouble());
        item.has_files = rec.value(QStringLiteral("_bHasFiles")).toBool(true);
        item.game_id = gid;
        item.target_game_id = target_game_id;

        if (rec.contains(QStringLiteral("_aSubmitter")))
        {
          item.submitter = rec.value(QStringLiteral("_aSubmitter"))
                               .toObject()
                               .value(QStringLiteral("_sName"))
                               .toString()
                               .toStdString();
        }
        if (rec.contains(QStringLiteral("_aRootCategory")))
        {
          item.category = rec.value(QStringLiteral("_aRootCategory"))
                              .toObject()
                              .value(QStringLiteral("_sName"))
                              .toString()
                              .toStdString();
        }
        else
        {
          item.category = rec.value(QStringLiteral("_sSingularTitle")).toString().toStdString();
        }

        if (rec.contains(QStringLiteral("_aGame")))
        {
          const QJsonObject gobj = rec.value(QStringLiteral("_aGame")).toObject();
          item.game_name = gobj.value(QStringLiteral("_sName")).toString().toStdString();
          const int rec_gid = gobj.value(QStringLiteral("_idRow")).toInt();
          if (rec_gid > 0)
            item.game_id = rec_gid;
        }
        if (item.target_game_id.empty() || item.game_name.empty())
        {
          for (const auto& cg : s_curated_games)
          {
            if (cg.gb_game_id == item.game_id && cg.gb_game_id > 0)
            {
              if (item.target_game_id.empty() && std::strlen(cg.game_id) > 0)
                item.target_game_id = cg.game_id;
              if (item.game_name.empty())
                item.game_name = cg.display_name;
              break;
            }
          }
        }

        if (rec.contains(QStringLiteral("_aPreviewMedia")))
        {
          const QJsonObject media = rec.value(QStringLiteral("_aPreviewMedia")).toObject();
          const QJsonArray images = media.value(QStringLiteral("_aImages")).toArray();
          if (!images.isEmpty())
          {
            const QJsonObject img0 = images.at(0).toObject();
            const QString base = img0.value(QStringLiteral("_sBaseUrl")).toString();
            const QString f530 = img0.value(QStringLiteral("_sFile530")).toString();
            const QString f220 = img0.value(QStringLiteral("_sFile220")).toString();
            const QString fraw = img0.value(QStringLiteral("_sFile")).toString();
            if (!base.isEmpty())
            {
              if (!f220.isEmpty())
                item.thumbnail_url = QString(base + QStringLiteral("/") + f220).toStdString();
              else if (!fraw.isEmpty())
                item.thumbnail_url = QString(base + QStringLiteral("/") + fraw).toStdString();

              if (!f530.isEmpty())
                item.full_image_url = QString(base + QStringLiteral("/") + f530).toStdString();
              else if (!fraw.isEmpty())
                item.full_image_url = QString(base + QStringLiteral("/") + fraw).toStdString();
            }
          }
        }

        if (search_term.size() == 1)
        {
          const QString hay =
              QString::fromStdString(item.name + " " + item.category + " " + item.submitter);
          if (!hay.contains(QString::fromStdString(search_term), Qt::CaseInsensitive))
            continue;
        }

        if (!cat_filter.empty())
        {
          const QString hay =
              QString::fromStdString(item.name + " " + item.category).toLower();
          if (!hay.contains(QString::fromStdString(cat_filter).toLower()))
            continue;
        }

        parsed_mods.push_back(std::move(item));
      }
    }

    if (!alive->load(std::memory_order_acquire) || !self)
      return;

    QueueOnObject(self.data(), [self, req_id, network_ok, mods = std::move(parsed_mods)]() mutable {
      if (!self || req_id != self->m_request_serial.load())
        return;

      self->m_search_btn->setEnabled(true);
      self->m_mods = std::move(mods);
      self->m_mods_table->setRowCount(0);
      self->m_selected_mod_index = -1;

      if (!network_ok)
      {
        self->m_status_label->setText(
            tr("Could not reach GameBanana API. Check your internet connection."));
        return;
      }

      self->m_mods_table->setRowCount(static_cast<int>(self->m_mods.size()));
      for (size_t i = 0; i < self->m_mods.size(); ++i)
      {
        const auto& m = self->m_mods[i];
        const int row = static_cast<int>(i);
        self->m_mods_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(m.name)));
        self->m_mods_table->setItem(row, 1,
                                    new QTableWidgetItem(QString::fromStdString(m.category)));
        self->m_mods_table->setItem(row, 2,
                                    new QTableWidgetItem(QString::fromStdString(m.game_name)));
        self->m_mods_table->setItem(row, 3,
                                    new QTableWidgetItem(QString::fromStdString(m.submitter)));
        self->m_mods_table->setItem(row, 4, new QTableWidgetItem(QString::number(m.likes)));
        self->m_mods_table->setItem(row, 5, new QTableWidgetItem(QString::number(m.views)));
      }

      self->m_status_label->setText(
          tr("Loaded %1 GameBanana mod(s). Select a row to inspect and install.")
              .arg(self->m_mods.size()));

      if (!self->m_mods.empty())
        self->m_mods_table->selectRow(0);
    });
  }).detach();
}

void GameBananaDialog::OnModSelected()
{
  const int row = m_mods_table->currentRow();
  if (row < 0 || static_cast<size_t>(row) >= m_mods.size())
  {
    m_selected_mod_index = -1;
    m_install_btn->setEnabled(false);
    m_open_browser_btn->setEnabled(false);
    return;
  }

  m_selected_mod_index = row;
  const ModItem& mod = m_mods[static_cast<size_t>(row)];

  m_detail_title->setText(QString::fromStdString(mod.name));
  m_detail_meta->setText(
      tr("By %1  |  Category: %2  |  Game: %3 (%4)")
          .arg(QString::fromStdString(mod.submitter.empty() ? "Unknown" : mod.submitter),
               QString::fromStdString(mod.category.empty() ? "Mod" : mod.category),
               QString::fromStdString(mod.game_name.empty() ? "GC/Wii" : mod.game_name),
               QString::fromStdString(mod.target_game_id.empty() ? "Auto" : mod.target_game_id)));
  m_detail_stats->setText(tr("Likes: %1   |   Views: %2   |   Downloads: %3")
                              .arg(mod.likes)
                              .arg(mod.views)
                              .arg(mod.downloads));
  m_open_browser_btn->setEnabled(!mod.profile_url.empty());

  m_preview_image_label->setPixmap(QPixmap());
  m_preview_image_label->setText(tr("Loading preview image..."));
  const std::string img_url =
      !mod.full_image_url.empty() ? mod.full_image_url : mod.thumbnail_url;
  if (!img_url.empty())
    FetchModImage(mod.id, img_url);
  else
    m_preview_image_label->setText(tr("No preview image available"));

  if (!mod.details_loaded)
  {
    m_detail_desc->setPlainText(tr("Loading full mod details and downloadable archives..."));
    m_files_combo->clear();
    m_install_btn->setEnabled(false);
    FetchModDetails(row);
  }
  else
  {
    m_detail_desc->setPlainText(QString::fromStdString(mod.description));
    m_files_combo->clear();
    for (const auto& f : mod.files)
    {
      m_files_combo->addItem(
          QStringLiteral("%1 (%2) - %3 dl")
              .arg(QString::fromStdString(f.filename), FormatBytes(f.filesize))
              .arg(f.downloads),
          QString::fromStdString(f.download_url));
    }
    m_install_btn->setEnabled(!mod.files.empty() && !m_is_downloading.load());
  }
}

void GameBananaDialog::FetchModDetails(int mod_index)
{
  if (mod_index < 0 || static_cast<size_t>(mod_index) >= m_mods.size())
    return;

  const int mod_id = m_mods[static_cast<size_t>(mod_index)].id;
  std::weak_ptr<std::atomic<bool>> weak_alive = m_alive;
  QPointer<GameBananaDialog> self(this);

  std::thread([self, weak_alive, mod_index, mod_id]() {
    auto alive = weak_alive.lock();
    if (!alive || !alive->load(std::memory_order_acquire))
      return;

    Common::HttpRequest http(std::chrono::milliseconds{15000},
                             [weak_alive](s64, s64, s64, s64) -> bool {
                               auto a = weak_alive.lock();
                               return a && a->load(std::memory_order_acquire);
                             });
    http.FollowRedirects(5);

    const std::string fast_url =
        "https://gamebanana.com/apiv11/Mod/" + std::to_string(mod_id) +
        "?_csvProperties=_sName,_sText,_sDescription,_aFiles,_nDownloadCount,_nLikeCount,"
        "_nViewCount,_sVersion,_aPreviewMedia";
    std::vector<u8> resp = FetchUrlBytes(http, fast_url);
    if (resp.empty() && alive->load(std::memory_order_acquire))
    {
      const std::string fallback_url =
          "https://gamebanana.com/apiv11/Mod/" + std::to_string(mod_id) + "/ProfilePage";
      resp = FetchUrlBytes(http, fallback_url);
    }
    if (resp.empty() || !alive->load(std::memory_order_acquire))
      return;

    const QByteArray bytes(reinterpret_cast<const char*>(resp.data()),
                           static_cast<qsizetype>(resp.size()));
    const QJsonDocument doc = QJsonDocument::fromJson(bytes);
    if (!doc.isObject())
      return;

    const QJsonObject root = doc.object();
    QString raw_text = root.value(QStringLiteral("_sText")).toString();
    if (raw_text.isEmpty())
      raw_text = root.value(QStringLiteral("_sDescription")).toString();
    const QString clean_desc = CleanHtmlDescription(raw_text);
    const int dl_count = root.value(QStringLiteral("_nDownloadCount")).toInt();

    std::vector<FileInfo> files;
    const QJsonArray files_arr = root.value(QStringLiteral("_aFiles")).toArray();
    for (const QJsonValue& fval : files_arr)
    {
      if (!fval.isObject())
        continue;
      const QJsonObject fobj = fval.toObject();
      FileInfo fi;
      fi.id = fobj.value(QStringLiteral("_idRow")).toInt();
      fi.filename = fobj.value(QStringLiteral("_sFile")).toString().toStdString();
      fi.download_url = fobj.value(QStringLiteral("_sDownloadUrl")).toString().toStdString();
      fi.description = fobj.value(QStringLiteral("_sDescription")).toString().toStdString();
      fi.filesize = static_cast<s64>(fobj.value(QStringLiteral("_nFilesize")).toDouble());
      fi.downloads = fobj.value(QStringLiteral("_nDownloadCount")).toInt();
      if (!fi.download_url.empty())
        files.push_back(std::move(fi));
    }

    QueueOnObject(self.data(), [self, mod_index, mod_id, clean_desc, dl_count,
                                files = std::move(files)]() mutable {
      if (!self || mod_index < 0 || static_cast<size_t>(mod_index) >= self->m_mods.size())
        return;

      ModItem& mod = self->m_mods[static_cast<size_t>(mod_index)];
      if (mod.id != mod_id)
        return;

      mod.details_loaded = true;
      mod.downloads = dl_count;
      mod.description =
          clean_desc.isEmpty() ? "No detailed description provided." : clean_desc.toStdString();
      mod.files = std::move(files);

      if (self->m_selected_mod_index == mod_index)
      {
        self->m_detail_stats->setText(tr("Likes: %1   |   Views: %2   |   Downloads: %3")
                                          .arg(mod.likes)
                                          .arg(mod.views)
                                          .arg(mod.downloads));
        self->m_detail_desc->setPlainText(QString::fromStdString(mod.description));
        self->m_files_combo->clear();
        for (const auto& f : mod.files)
        {
          self->m_files_combo->addItem(
              QStringLiteral("%1 (%2) - %3 dl")
                  .arg(QString::fromStdString(f.filename), FormatBytes(f.filesize))
                  .arg(f.downloads),
              QString::fromStdString(f.download_url));
        }
        if (mod.files.empty())
        {
          self->m_files_combo->addItem(tr("No direct archive files listed (open in browser)"));
          self->m_install_btn->setEnabled(false);
        }
        else
        {
          self->m_install_btn->setEnabled(!self->m_is_downloading.load());
        }
      }
    });
  }).detach();
}

void GameBananaDialog::FetchModImage(int mod_id, const std::string& url)
{
  std::weak_ptr<std::atomic<bool>> weak_alive = m_alive;
  QPointer<GameBananaDialog> self(this);

  std::thread([self, weak_alive, mod_id, url]() {
    auto alive = weak_alive.lock();
    if (!alive || !alive->load(std::memory_order_acquire))
      return;

    Common::HttpRequest http(std::chrono::milliseconds{10000},
                             [weak_alive](s64, s64, s64, s64) -> bool {
                               auto a = weak_alive.lock();
                               return a && a->load(std::memory_order_acquire);
                             });
    http.FollowRedirects(5);

    const std::vector<u8> resp =
        FetchUrlBytes(http, url, {{"User-Agent", "DolphinXR-GameBanana/0.4"}});
    if (resp.empty() || !alive->load(std::memory_order_acquire))
      return;

    QImage image;
    if (!image.loadFromData(resp.data(), static_cast<int>(resp.size())))
      return;

    QueueOnObject(self.data(), [self, mod_id, image = std::move(image)]() {
      if (!self || self->m_selected_mod_index < 0 ||
          static_cast<size_t>(self->m_selected_mod_index) >= self->m_mods.size())
      {
        return;
      }
      if (self->m_mods[static_cast<size_t>(self->m_selected_mod_index)].id != mod_id)
        return;

      const QPixmap pix = QPixmap::fromImage(image).scaled(
          self->m_preview_image_label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
      self->m_preview_image_label->setText(QString());
      self->m_preview_image_label->setPixmap(pix);
    });
  }).detach();
}

void GameBananaDialog::OnOpenInBrowser()
{
  if (m_selected_mod_index < 0 || static_cast<size_t>(m_selected_mod_index) >= m_mods.size())
    return;

  const std::string& url = m_mods[static_cast<size_t>(m_selected_mod_index)].profile_url;
  if (!url.empty())
    QDesktopServices::openUrl(QUrl(QString::fromStdString(url)));
}

void GameBananaDialog::OnDownloadAndInstall()
{
  if (m_is_downloading.load() || m_selected_mod_index < 0 ||
      static_cast<size_t>(m_selected_mod_index) >= m_mods.size())
  {
    return;
  }

  const ModItem& mod = m_mods[static_cast<size_t>(m_selected_mod_index)];
  const int file_idx = m_files_combo->currentIndex();
  if (file_idx < 0 || static_cast<size_t>(file_idx) >= mod.files.size())
    return;

  const FileInfo file_info = mod.files[static_cast<size_t>(file_idx)];
  const std::string mod_name = mod.name;
  const std::string target_game_id =
      !mod.target_game_id.empty() ? mod.target_game_id : GetSelectedTargetGameID();

  m_is_downloading.store(true);
  m_install_btn->setEnabled(false);
  m_download_progress->setValue(0);
  m_status_label->setText(
      tr("Downloading %1...").arg(QString::fromStdString(file_info.filename)));

  std::weak_ptr<std::atomic<bool>> weak_alive = m_alive;
  QPointer<GameBananaDialog> self(this);

  std::thread([self, weak_alive, file_info, mod_name, target_game_id]() {
    auto alive = weak_alive.lock();
    if (!alive || !alive->load(std::memory_order_acquire))
      return;

    Common::HttpRequest http(
        std::chrono::milliseconds{180000},
        [self, weak_alive](s64 dltotal, s64 dlnow, s64, s64) -> bool {
          auto a = weak_alive.lock();
          if (!a || !a->load(std::memory_order_acquire))
            return false;
          if (dltotal > 0 && self)
          {
            const int pct = static_cast<int>((dlnow * 100) / dltotal);
            QueueOnObject(self.data(), [self, pct]() {
              if (self)
                self->m_download_progress->setValue(std::clamp(pct, 0, 100));
            });
          }
          return true;
        });
    http.FollowRedirects(10);

    const std::vector<u8> resp = FetchUrlBytes(
        http, file_info.download_url, {{"User-Agent", "DolphinXR-GameBanana/0.4"}});
    if (!alive->load(std::memory_order_acquire) || !self)
      return;

    if (resp.empty())
    {
      QueueOnObject(self.data(), [self]() {
        if (!self)
          return;
        self->m_is_downloading.store(false);
        self->m_install_btn->setEnabled(true);
        self->m_status_label->setText(tr("Download failed."));
        ModalMessageBox::warning(self, tr("Download Failed"),
                                 tr("Failed to download the selected mod archive from GameBanana."));
      });
      return;
    }

    const std::string dl_dir = File::GetUserPath(D_LOAD_IDX) + "GameBananaDownloads/";
    File::CreateFullPath(dl_dir);
    const std::string archive_path = dl_dir + SanitizeFolderName(file_info.filename);

    {
      File::IOFile out_file(archive_path, "wb");
      if (!out_file || !out_file.WriteBytes(resp.data(), resp.size()))
      {
        QueueOnObject(self.data(), [self]() {
          if (!self)
            return;
          self->m_is_downloading.store(false);
          self->m_install_btn->setEnabled(true);
          self->m_status_label->setText(tr("Failed to write downloaded archive to disk."));
        });
        return;
      }
    }

    QueueOnObject(self.data(), [self, archive_path, mod_name, target_game_id]() {
      if (!self)
        return;

      self->m_download_progress->setValue(100);
      QString summary;
      const bool ok =
          self->InstallArchiveFile(archive_path, mod_name, target_game_id, &summary);
      self->m_is_downloading.store(false);
      self->m_install_btn->setEnabled(true);
      self->RefreshInstalledMods();

      if (ok)
      {
        self->m_status_label->setText(tr("Installed '%1' successfully.")
                                          .arg(QString::fromStdString(mod_name)));
        ModalMessageBox::information(self, tr("Mod Installed"), summary);
      }
      else
      {
        self->m_status_label->setText(tr("Mod downloaded to Load/GameBananaDownloads."));
        ModalMessageBox::warning(self, tr("Archive Saved"), summary);
      }
    });
  }).detach();
}

bool GameBananaDialog::InstallArchiveFile(const std::string& archive_path,
                                          const std::string& mod_name,
                                          const std::string& target_game_id, QString* out_summary)
{
  const QString qpath = QString::fromStdString(archive_path);
  const std::string effective_game_id = target_game_id.empty() ? "GLOBAL" : target_game_id;

  if (!qpath.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive))
  {
    const std::string dest_dir = File::GetUserPath(D_LOAD_IDX) + "GameBananaMods/" +
                                 effective_game_id + "/" + SanitizeFolderName(mod_name) + "/";
    File::CreateFullPath(dest_dir);
    const std::string filename = QFileInfo(qpath).fileName().toStdString();
    const std::string dest_file = dest_dir + filename;
    File::Copy(archive_path, dest_file);

    if (out_summary)
    {
      *out_summary =
          tr("Saved archive '%1' to:\n%2\n\n(Note: .7z / .rar archives can be extracted there or "
             "opened via 'Installed Mods & Texture Packs'.)")
              .arg(QString::fromStdString(filename), QString::fromStdString(dest_dir));
    }
    return true;
  }

  void* zip_reader = mz_zip_reader_create();
  if (!zip_reader)
  {
    if (out_summary)
      *out_summary = tr("Failed to initialize ZIP reader.");
    return false;
  }
  Common::ScopeGuard zip_guard{[&] { mz_zip_reader_delete(&zip_reader); }};

  if (mz_zip_reader_open_file(zip_reader, archive_path.c_str()) != MZ_OK)
  {
    if (out_summary)
      *out_summary = tr("Could not open ZIP archive: %1").arg(qpath);
    return false;
  }

  bool has_resource_manifest = false;
  bool has_textures_dir = false;
  bool has_graphics_mod_metadata = false;
  bool has_riivolution_xml = false;
  bool has_custom_textures = false;
  bool starts_with_gameid_folder = false;

  if (mz_zip_reader_goto_first_entry(zip_reader) == MZ_OK)
  {
    do
    {
      mz_zip_file* info = nullptr;
      if (mz_zip_reader_entry_get_info(zip_reader, &info) != MZ_OK || !info || !info->filename)
        continue;

      std::string entry_name(info->filename);
      std::ranges::replace(entry_name, '\\', '/');
      std::string lower = entry_name;
      Common::ToLower(&lower);

      if (lower == "manifest.json")
        has_resource_manifest = true;
      if (lower.starts_with("textures/"))
        has_textures_dir = true;
      if (lower == "metadata.json" || lower.ends_with("/metadata.json"))
        has_graphics_mod_metadata = true;
      if (lower.starts_with("riivolution/") ||
          (lower.find("riivolution/") != std::string::npos && lower.ends_with(".xml")))
      {
        has_riivolution_xml = true;
      }
      if (lower.ends_with(".png") || lower.ends_with(".dds"))
      {
        has_custom_textures = true;
        const size_t slash = entry_name.find('/');
        if (slash != std::string::npos)
        {
          const std::string first_seg = entry_name.substr(0, slash);
          if (LooksLikeGameIDFolder(first_seg))
            starts_with_gameid_folder = true;
        }
      }
    } while (mz_zip_reader_goto_next_entry(zip_reader) == MZ_OK);
  }

  // Case 1: Official Dolphin Resource Pack (manifest.json + textures/)
  if (has_resource_manifest && has_textures_dir)
  {
    const std::string rp_dir = File::GetUserPath(D_RESOURCEPACK_IDX);
    File::CreateFullPath(rp_dir);
    const std::string dest_zip = rp_dir + SanitizeFolderName(mod_name) + ".zip";
    File::Copy(archive_path, dest_zip);

    ResourcePack::ResourcePack* pack = ResourcePack::Add(dest_zip);
    if (pack && pack->Install(File::GetUserPath(D_LOAD_IDX)))
    {
      Config::SetBaseOrCurrent(Config::GFX_HIRES_TEXTURES, true);
      Config::Save();
      m_chk_hires_textures->setChecked(true);
      if (out_summary)
      {
        *out_summary = tr("Installed Dolphin Resource Pack '%1' to:\n%2\n\nCustom Textures have "
                          "been enabled automatically.")
                           .arg(QString::fromStdString(mod_name), QString::fromStdString(dest_zip));
      }
      return true;
    }
  }

  // Determine destination base directory for extraction
  std::string base_extract_dir;
  QString install_kind;

  if (has_graphics_mod_metadata)
  {
    base_extract_dir =
        File::GetUserPath(D_GRAPHICSMOD_IDX) + SanitizeFolderName(mod_name) + "/";
    install_kind = tr("Graphics Mod");
    Config::SetBaseOrCurrent(Config::GFX_MODS_ENABLE, true);
    Config::Save();
    m_chk_graphics_mods->setChecked(true);
  }
  else if (has_riivolution_xml)
  {
    base_extract_dir = File::GetUserPath(D_RIIVOLUTION_IDX);
    install_kind = tr("Riivolution Mod");
  }
  else if (has_custom_textures)
  {
    if (starts_with_gameid_folder)
    {
      base_extract_dir = File::GetUserPath(D_HIRESTEXTURES_IDX);
    }
    else
    {
      const std::string tex_folder =
          !target_game_id.empty() ? target_game_id : SanitizeFolderName(mod_name);
      base_extract_dir = File::GetUserPath(D_HIRESTEXTURES_IDX) + tex_folder + "/";
    }
    install_kind = tr("Custom HD Texture Pack");
    Config::SetBaseOrCurrent(Config::GFX_HIRES_TEXTURES, true);
    Config::Save();
    m_chk_hires_textures->setChecked(true);
  }
  else
  {
    base_extract_dir = File::GetUserPath(D_LOAD_IDX) + "GameBananaMods/" + effective_game_id +
                       "/" + SanitizeFolderName(mod_name) + "/";
    install_kind = tr("GameBanana Mod Package");
  }

  File::CreateFullPath(base_extract_dir);

  int extracted_files = 0;
  if (mz_zip_reader_goto_first_entry(zip_reader) == MZ_OK)
  {
    do
    {
      mz_zip_file* info = nullptr;
      if (mz_zip_reader_entry_get_info(zip_reader, &info) != MZ_OK || !info || !info->filename)
        continue;

      std::string rel_path(info->filename);
      std::ranges::replace(rel_path, '\\', '/');
      if (rel_path.empty() || rel_path.find("..") != std::string::npos)
        continue;

      while (!rel_path.empty() && rel_path.front() == '/')
        rel_path.erase(rel_path.begin());

      const std::string out_path = base_extract_dir + rel_path;
      if (mz_zip_reader_entry_is_dir(zip_reader) == MZ_OK || rel_path.ends_with('/'))
      {
        File::CreateFullPath(out_path);
        continue;
      }

      std::string parent_dir;
      if (SplitPath(out_path, &parent_dir, nullptr, nullptr))
        File::CreateFullPath(parent_dir);

      const size_t uncompressed_size = static_cast<size_t>(info->uncompressed_size);
      std::vector<u8> buffer(uncompressed_size);
      if (uncompressed_size > 0 &&
          !Common::ReadFileFromZip(zip_reader, buffer.data(), uncompressed_size))
      {
        continue;
      }

      File::IOFile out(out_path, "wb");
      if (out && (uncompressed_size == 0 || out.WriteBytes(buffer.data(), uncompressed_size)))
        ++extracted_files;

    } while (mz_zip_reader_goto_next_entry(zip_reader) == MZ_OK);
  }

  if (out_summary)
  {
    *out_summary = tr("Installed '%1' as %2 (%3 file(s) extracted).\n\nLocation:\n%4")
                       .arg(QString::fromStdString(mod_name), install_kind)
                       .arg(extracted_files)
                       .arg(QString::fromStdString(base_extract_dir));
  }
  return extracted_files > 0;
}

void GameBananaDialog::RefreshInstalledMods()
{
  m_installed_mods.clear();

  // 1. Resource Packs
  auto& packs = ResourcePack::GetPacks();
  for (size_t i = 0; i < packs.size(); ++i)
  {
    const auto& pack = packs[i];
    if (!pack.IsValid() || !pack.GetManifest())
      continue;
    InstalledModEntry entry;
    entry.name = pack.GetManifest()->GetName();
    entry.type = "Resource Pack";
    entry.game_id = "ALL";
    entry.path = pack.GetPath();
    entry.enabled = ResourcePack::IsInstalled(pack);
    entry.is_resource_pack = true;
    entry.resource_pack_index = i;
    m_installed_mods.push_back(std::move(entry));
  }

  // Helper to scan directory children
  const auto scan_subdirs = [this](const std::string& root_dir, const std::string& type_label) {
    std::error_code ec;
    if (!std::filesystem::exists(root_dir, ec))
      return;
    for (const auto& dir_entry : std::filesystem::directory_iterator(root_dir, ec))
    {
      if (!dir_entry.is_directory(ec))
        continue;
      std::string folder_name = dir_entry.path().filename().string();
      bool enabled = true;
      if (folder_name.ends_with(".disabled"))
      {
        enabled = false;
        folder_name.resize(folder_name.size() - 9);
      }

      InstalledModEntry item;
      item.name = folder_name;
      item.type = type_label;
      item.game_id = LooksLikeGameIDFolder(folder_name) ? folder_name : "";
      item.path = dir_entry.path().string();
      item.enabled = enabled;
      m_installed_mods.push_back(std::move(item));
    }
  };

  // 2. Custom Texture Packs (Load/Textures)
  scan_subdirs(File::GetUserPath(D_HIRESTEXTURES_IDX), "Custom Textures");

  // 3. Graphics Mods (Load/GraphicMods)
  scan_subdirs(File::GetUserPath(D_GRAPHICSMOD_IDX), "Graphics Mod");

  // 4. Riivolution Mods (Load/Riivolution)
  scan_subdirs(File::GetUserPath(D_RIIVOLUTION_IDX), "Riivolution Mod");

  // 5. General GameBanana Mods (Load/GameBananaMods/<GameID>/<ModName>)
  const std::string gb_mods_root = File::GetUserPath(D_LOAD_IDX) + "GameBananaMods/";
  std::error_code ec;
  if (std::filesystem::exists(gb_mods_root, ec))
  {
    for (const auto& game_dir : std::filesystem::directory_iterator(gb_mods_root, ec))
    {
      if (!game_dir.is_directory(ec))
        continue;
      const std::string gid = game_dir.path().filename().string();
      for (const auto& mod_dir : std::filesystem::directory_iterator(game_dir.path(), ec))
      {
        if (!mod_dir.is_directory(ec))
          continue;
        std::string mod_folder = mod_dir.path().filename().string();
        bool enabled = true;
        if (mod_folder.ends_with(".disabled"))
        {
          enabled = false;
          mod_folder.resize(mod_folder.size() - 9);
        }
        InstalledModEntry item;
        item.name = mod_folder;
        item.type = "GameBanana Mod";
        item.game_id = gid;
        item.path = mod_dir.path().string();
        item.enabled = enabled;
        m_installed_mods.push_back(std::move(item));
      }
    }
  }

  m_installed_table->setRowCount(static_cast<int>(m_installed_mods.size()));
  for (size_t i = 0; i < m_installed_mods.size(); ++i)
  {
    const auto& m = m_installed_mods[i];
    const int row = static_cast<int>(i);
    m_installed_table->setItem(row, 0,
                               new QTableWidgetItem(m.enabled ? tr("Enabled") : tr("Disabled")));
    m_installed_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(m.name)));
    m_installed_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(m.type)));
    m_installed_table->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(m.game_id)));
    m_installed_table->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(m.path)));
  }
}

void GameBananaDialog::OnToggleInstalledMod()
{
  const int row = m_installed_table->currentRow();
  if (row < 0 || static_cast<size_t>(row) >= m_installed_mods.size())
    return;

  InstalledModEntry& entry = m_installed_mods[static_cast<size_t>(row)];
  if (entry.is_resource_pack)
  {
    auto& packs = ResourcePack::GetPacks();
    if (entry.resource_pack_index < packs.size())
    {
      auto& pack = packs[entry.resource_pack_index];
      if (entry.enabled)
        pack.Uninstall(File::GetUserPath(D_LOAD_IDX));
      else
        pack.Install(File::GetUserPath(D_LOAD_IDX));
    }
    RefreshInstalledMods();
    return;
  }

  std::string new_path;
  if (entry.enabled)
  {
    new_path = entry.path + ".disabled";
  }
  else if (entry.path.ends_with(".disabled"))
  {
    new_path = entry.path.substr(0, entry.path.size() - 9);
  }

  if (!new_path.empty())
  {
    std::error_code ec;
    std::filesystem::rename(entry.path, new_path, ec);
    if (ec)
    {
      ModalMessageBox::warning(this, tr("Rename Failed"),
                               tr("Could not toggle mod folder state:\n%1")
                                   .arg(QString::fromStdString(ec.message())));
    }
    RefreshInstalledMods();
  }
}

void GameBananaDialog::OnImportLocalMod()
{
  const QString file = DolphinFileDialog::getOpenFileName(
      this, tr("Select Mod or Texture Pack Archive"), QString(),
      tr("Mod Archives (*.zip *.7z *.rar);;All Files (*)"));
  if (file.isEmpty())
    return;

  const std::string mod_name = QFileInfo(file).completeBaseName().toStdString();
  QString summary;
  if (InstallArchiveFile(file.toStdString(), mod_name, GetSelectedTargetGameID(), &summary))
  {
    RefreshInstalledMods();
    ModalMessageBox::information(this, tr("Mod Imported"), summary);
  }
  else
  {
    ModalMessageBox::warning(this, tr("Import Failed"), summary);
  }
}

void GameBananaDialog::OnOpenSelectedModFolder()
{
  const int row = m_installed_table->currentRow();
  if (row < 0 || static_cast<size_t>(row) >= m_installed_mods.size())
    return;

  const InstalledModEntry& entry = m_installed_mods[static_cast<size_t>(row)];
  QFileInfo fi(QString::fromStdString(entry.path));
  const QString folder = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
  QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void GameBananaDialog::OnOpenTexturesFolder()
{
  const std::string path = File::GetUserPath(D_HIRESTEXTURES_IDX);
  File::CreateFullPath(path);
  QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path)));
}

void GameBananaDialog::OnDeleteInstalledMod()
{
  const int row = m_installed_table->currentRow();
  if (row < 0 || static_cast<size_t>(row) >= m_installed_mods.size())
    return;

  InstalledModEntry& entry = m_installed_mods[static_cast<size_t>(row)];
  const auto confirm = ModalMessageBox::question(
      this, tr("Confirm Uninstall"),
      tr("Are you sure you want to permanently remove '%1'?").arg(QString::fromStdString(entry.name)),
      QMessageBox::Yes | QMessageBox::No);
  if (confirm != QMessageBox::Yes)
    return;

  if (entry.is_resource_pack)
  {
    auto& packs = ResourcePack::GetPacks();
    if (entry.resource_pack_index < packs.size())
    {
      ResourcePack::Remove(packs[entry.resource_pack_index]);
      File::Delete(entry.path);
    }
  }
  else
  {
    std::error_code ec;
    std::filesystem::remove_all(entry.path, ec);
  }

  RefreshInstalledMods();
}

void GameBananaDialog::PopulateCheatTemplates()
{
  m_templates_table->setRowCount(static_cast<int>(s_cheat_templates.size()));
  for (size_t i = 0; i < s_cheat_templates.size(); ++i)
  {
    const auto& tpl = s_cheat_templates[i];
    const int row = static_cast<int>(i);
    m_templates_table->setItem(row, 0, new QTableWidgetItem(QString::fromUtf8(tpl.name)));
    m_templates_table->setItem(row, 1, new QTableWidgetItem(QString::fromUtf8(tpl.game_name)));
    m_templates_table->setItem(row, 2, new QTableWidgetItem(QString::fromLatin1(tpl.game_id)));
    m_templates_table->setItem(row, 3, new QTableWidgetItem(QString::fromLatin1(tpl.code_type)));
  }
}

void GameBananaDialog::OnApplyTemplate()
{
  const int row = m_templates_table->currentRow();
  if (row < 0 || static_cast<size_t>(row) >= s_cheat_templates.size())
    return;

  const auto& tpl = s_cheat_templates[static_cast<size_t>(row)];
  m_cheat_game_id_edit->setText(QString::fromLatin1(tpl.game_id));
  m_cheat_name_edit->setText(QString::fromUtf8(tpl.name));
  m_cheat_creator_edit->setText(QString::fromUtf8(tpl.creator));
  m_cheat_notes_edit->setText(QString::fromUtf8(tpl.description));
  m_cheat_type_combo->setCurrentIndex(std::strcmp(tpl.code_type, "AR") == 0 ? 1 : 0);
  m_cheat_lines_edit->setPlainText(QString::fromLatin1(tpl.code_lines));
}

void GameBananaDialog::RefreshExistingCodes()
{
  m_existing_codes_list->clear();
  const std::string game_id = m_cheat_game_id_edit->text().trimmed().toUpper().toStdString();
  if (game_id.empty())
    return;

  const std::string ini_path = File::GetUserPath(D_GAMESETTINGS_IDX) + game_id + ".ini";
  Common::IniFile local_ini;
  local_ini.Load(ini_path);
  const Common::IniFile default_ini = SConfig::LoadDefaultGameIni(game_id, 0);

  const auto gecko_codes = Gecko::LoadCodes(default_ini, local_ini);
  for (size_t i = 0; i < gecko_codes.size(); ++i)
  {
    const auto& gc = gecko_codes[i];
    auto* item = new QListWidgetItem(
        QStringLiteral("[Gecko] %1").arg(QString::fromStdString(gc.name)), m_existing_codes_list);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    item->setCheckState(gc.enabled ? Qt::Checked : Qt::Unchecked);
    item->setData(Qt::UserRole, QStringLiteral("Gecko"));
    item->setData(Qt::UserRole + 1, static_cast<int>(i));
  }

  const auto ar_codes = ActionReplay::LoadCodes(default_ini, local_ini);
  for (size_t i = 0; i < ar_codes.size(); ++i)
  {
    const auto& ar = ar_codes[i];
    auto* item = new QListWidgetItem(
        QStringLiteral("[AR] %1").arg(QString::fromStdString(ar.name)), m_existing_codes_list);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    item->setCheckState(ar.enabled ? Qt::Checked : Qt::Unchecked);
    item->setData(Qt::UserRole, QStringLiteral("AR"));
    item->setData(Qt::UserRole + 1, static_cast<int>(i));
  }
}

void GameBananaDialog::OnSaveToggledExistingCodes()
{
  const std::string game_id = m_cheat_game_id_edit->text().trimmed().toUpper().toStdString();
  if (game_id.empty())
    return;

  const std::string ini_path = File::GetUserPath(D_GAMESETTINGS_IDX) + game_id + ".ini";
  File::CreateFullPath(File::GetUserPath(D_GAMESETTINGS_IDX));

  Common::IniFile local_ini;
  local_ini.Load(ini_path);
  const Common::IniFile default_ini = SConfig::LoadDefaultGameIni(game_id, 0);

  auto gecko_codes = Gecko::LoadCodes(default_ini, local_ini);
  auto ar_codes = ActionReplay::LoadCodes(default_ini, local_ini);

  for (int row = 0; row < m_existing_codes_list->count(); ++row)
  {
    const QListWidgetItem* item = m_existing_codes_list->item(row);
    const QString kind = item->data(Qt::UserRole).toString();
    const int idx = item->data(Qt::UserRole + 1).toInt();
    const bool checked = (item->checkState() == Qt::Checked);

    if (kind == QStringLiteral("Gecko") && idx >= 0 &&
        static_cast<size_t>(idx) < gecko_codes.size())
    {
      gecko_codes[static_cast<size_t>(idx)].enabled = checked;
    }
    else if (kind == QStringLiteral("AR") && idx >= 0 &&
             static_cast<size_t>(idx) < ar_codes.size())
    {
      ar_codes[static_cast<size_t>(idx)].enabled = checked;
    }
  }

  Gecko::SaveCodes(local_ini, gecko_codes);
  ActionReplay::SaveCodes(&local_ini, ar_codes);
  local_ini.Save(ini_path);

  if (Core::IsRunning(Core::System::GetInstance()) &&
      SConfig::GetInstance().GetGameID() == game_id)
  {
    Gecko::SetActiveCodes(gecko_codes, game_id, SConfig::GetInstance().GetRevision());
    ActionReplay::ApplyCodes(ar_codes, game_id, SConfig::GetInstance().GetRevision());
  }

  ModalMessageBox::information(
      this, tr("Cheats Updated"),
      tr("Saved updated cheat states to:\n%1").arg(QString::fromStdString(ini_path)));
}

void GameBananaDialog::OnAppendQuickPatch()
{
  bool ok_addr = false;
  bool ok_val = false;
  const u32 raw_addr = m_patch_addr_edit->text().trimmed().toUInt(&ok_addr, 16);
  const u32 raw_val = m_patch_val_edit->text().trimmed().toUInt(&ok_val, 16);
  if (!ok_addr || !ok_val)
  {
    ModalMessageBox::warning(this, tr("Invalid Hex"),
                             tr("Please enter valid hexadecimal RAM address and value."));
    return;
  }

  const QString prefix = m_patch_type_combo->currentData().toString();
  const u32 offset = raw_addr & 0x01FFFFFFu;
  u32 cmd_prefix = 0x04000000u;
  if (prefix == QStringLiteral("02"))
    cmd_prefix = 0x02000000u;
  else if (prefix == QStringLiteral("00"))
    cmd_prefix = 0x00000000u;

  const u32 word1 = cmd_prefix | offset;
  const QString line = QStringLiteral("%1 %2")
                           .arg(word1, 8, 16, QLatin1Char('0'))
                           .arg(raw_val, 8, 16, QLatin1Char('0'))
                           .toUpper();
  m_cheat_lines_edit->appendPlainText(line);
}

void GameBananaDialog::OnSaveNewCheatCode()
{
  const std::string game_id = m_cheat_game_id_edit->text().trimmed().toUpper().toStdString();
  const std::string code_name = m_cheat_name_edit->text().trimmed().toStdString();
  const std::string creator = m_cheat_creator_edit->text().trimmed().toStdString();
  const std::string notes = m_cheat_notes_edit->text().trimmed().toStdString();
  const QString code_type = m_cheat_type_combo->currentData().toString();
  const QString raw_lines = m_cheat_lines_edit->toPlainText().trimmed();

  if (game_id.empty() || code_name.empty() || raw_lines.isEmpty())
  {
    ModalMessageBox::warning(
        this, tr("Missing Fields"),
        tr("Please specify a 6-character Game ID, a Code Name, and at least one code line."));
    return;
  }

  const std::string ini_dir = File::GetUserPath(D_GAMESETTINGS_IDX);
  File::CreateFullPath(ini_dir);
  const std::string ini_path = ini_dir + game_id + ".ini";

  Common::IniFile local_ini;
  local_ini.Load(ini_path);
  const Common::IniFile default_ini = SConfig::LoadDefaultGameIni(game_id, 0);

  if (code_type == QStringLiteral("Gecko"))
  {
    Gecko::GeckoCode new_code;
    new_code.name = code_name;
    new_code.creator = creator;
    new_code.enabled = true;
    new_code.user_defined = true;
    if (!notes.empty())
      new_code.notes.push_back(notes);

    const QStringList lines = raw_lines.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& qline : lines)
    {
      const std::string sline = qline.trimmed().toStdString();
      if (sline.empty())
        continue;
      if (std::optional<Gecko::GeckoCode::Code> parsed = Gecko::DeserializeLine(sline))
      {
        new_code.codes.push_back(*parsed);
      }
      else
      {
        ModalMessageBox::warning(
            this, tr("Invalid Gecko Line"),
            tr("Could not parse Gecko code line:\n%1\n\nExpected format: XXXXXXXX YYYYYYYY")
                .arg(qline));
        return;
      }
    }

    auto gecko_codes = Gecko::LoadCodes(default_ini, local_ini);
    auto existing_it = std::ranges::find_if(
        gecko_codes, [&](const Gecko::GeckoCode& c) { return c.name == new_code.name; });
    if (existing_it != gecko_codes.end())
      *existing_it = std::move(new_code);
    else
      gecko_codes.push_back(std::move(new_code));

    Gecko::SaveCodes(local_ini, gecko_codes);
    local_ini.Save(ini_path);

    if (Core::IsRunning(Core::System::GetInstance()) &&
        SConfig::GetInstance().GetGameID() == game_id)
    {
      Gecko::SetActiveCodes(gecko_codes, game_id, SConfig::GetInstance().GetRevision());
    }
  }
  else
  {
    ActionReplay::ARCode new_ar;
    new_ar.name = code_name;
    new_ar.enabled = true;
    new_ar.user_defined = true;

    const QStringList lines = raw_lines.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& qline : lines)
    {
      const std::string sline = qline.trimmed().toStdString();
      if (sline.empty())
        continue;
      const auto parsed = ActionReplay::DeserializeLine(sline);
      if (std::holds_alternative<ActionReplay::AREntry>(parsed))
      {
        new_ar.ops.push_back(std::get<ActionReplay::AREntry>(parsed));
      }
      else
      {
        ModalMessageBox::warning(
            this, tr("Invalid Action Replay Line"),
            tr("Could not parse Action Replay line:\n%1\n\nExpected unencrypted hex format: "
               "XXXXXXXX YYYYYYYY")
                .arg(qline));
        return;
      }
    }

    auto ar_codes = ActionReplay::LoadCodes(default_ini, local_ini);
    auto existing_it = std::ranges::find_if(
        ar_codes, [&](const ActionReplay::ARCode& c) { return c.name == new_ar.name; });
    if (existing_it != ar_codes.end())
      *existing_it = std::move(new_ar);
    else
      ar_codes.push_back(std::move(new_ar));

    ActionReplay::SaveCodes(&local_ini, ar_codes);
    local_ini.Save(ini_path);

    if (Core::IsRunning(Core::System::GetInstance()) &&
        SConfig::GetInstance().GetGameID() == game_id)
    {
      ActionReplay::ApplyCodes(ar_codes, game_id, SConfig::GetInstance().GetRevision());
    }
  }

  Config::SetBaseOrCurrent(Config::MAIN_ENABLE_CHEATS, true);
  Config::Save();
  m_chk_enable_cheats->setChecked(true);
  RefreshExistingCodes();

  ModalMessageBox::information(
      this, tr("Cheat Code Saved"),
      tr("Saved and enabled '%1' in:\n%2").arg(QString::fromStdString(code_name),
                                               QString::fromStdString(ini_path)));
}

void GameBananaDialog::OnOpenGameIni()
{
  const std::string game_id = m_cheat_game_id_edit->text().trimmed().toUpper().toStdString();
  if (game_id.empty())
    return;

  const std::string ini_dir = File::GetUserPath(D_GAMESETTINGS_IDX);
  File::CreateFullPath(ini_dir);
  const std::string ini_path = ini_dir + game_id + ".ini";
  if (!File::Exists(ini_path))
  {
    File::IOFile f(ini_path, "wb");
  }
  QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(ini_path)));
}
