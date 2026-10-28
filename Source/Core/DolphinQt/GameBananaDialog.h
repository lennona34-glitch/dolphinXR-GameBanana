// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include <QDialog>

#include "Common/CommonTypes.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QShowEvent;
class QTableWidget;
class QTabWidget;
class QTextBrowser;

class GameBananaDialog final : public QDialog
{
  Q_OBJECT

public:
  struct FileInfo
  {
    int id = 0;
    std::string filename;
    std::string download_url;
    std::string description;
    s64 filesize = 0;
    int downloads = 0;
  };

  struct ModItem
  {
    int id = 0;
    std::string model_name = "Mod";
    std::string name;
    std::string submitter;
    std::string category;
    std::string super_category;
    std::string game_name;
    int game_id = 0;
    std::string target_game_id;
    std::string profile_url;
    std::string thumbnail_url;
    std::string full_image_url;
    std::string version;
    std::string description;
    int likes = 0;
    int views = 0;
    int downloads = 0;
    s64 date_added = 0;
    s64 date_updated = 0;
    bool has_files = false;
    bool details_loaded = false;
    std::vector<FileInfo> files;
  };

  struct InstalledModEntry
  {
    std::string name;
    std::string type;
    std::string game_id;
    std::string path;
    bool enabled = true;
    bool is_resource_pack = false;
    size_t resource_pack_index = 0;
  };

  explicit GameBananaDialog(QWidget* parent = nullptr);
  ~GameBananaDialog() override;

  void SelectGameByGameID(const std::string& game_id, const std::string& game_title = {});

protected:
  void showEvent(QShowEvent* event) override;

private:
  void CreateWidgets();
  QWidget* CreateBrowseTab();
  QWidget* CreateInstalledTab();
  QWidget* CreateCheatsTab();
  void ConnectWidgets();

  // Tab 1: Browse & Download GameBanana Mods
  void TriggerSearch(int page = 1);
  void OnModSelected();
  void FetchModDetails(int mod_index);
  void FetchModImage(int mod_id, const std::string& url);
  void OnDownloadAndInstall();
  void OnOpenInBrowser();
  bool InstallArchiveFile(const std::string& archive_path, const std::string& mod_name,
                          const std::string& target_game_id, QString* out_summary);

  // Tab 2: Installed Mods, Texture Packs & Resource Packs
  void RefreshInstalledMods();
  void OnToggleInstalledMod();
  void OnImportLocalMod();
  void OnOpenSelectedModFolder();
  void OnOpenTexturesFolder();
  void OnDeleteInstalledMod();

  // Tab 3: Cheats & Gecko / AR Codes Studio
  void PopulateCheatTemplates();
  void RefreshExistingCodes();
  void OnSaveToggledExistingCodes();
  void OnApplyTemplate();
  void OnAppendQuickPatch();
  void OnSaveNewCheatCode();
  void OnOpenGameIni();

  std::string GetSelectedTargetGameID() const;

  std::shared_ptr<std::atomic<bool>> m_alive;
  std::atomic<int> m_request_serial{0};
  std::atomic<bool> m_is_downloading{false};

  QTabWidget* m_tabs = nullptr;

  // Browse Tab Widgets
  QComboBox* m_game_combo = nullptr;
  QComboBox* m_category_combo = nullptr;
  QComboBox* m_sort_combo = nullptr;
  QLineEdit* m_search_edit = nullptr;
  QPushButton* m_search_btn = nullptr;
  QPushButton* m_prev_page_btn = nullptr;
  QPushButton* m_next_page_btn = nullptr;
  QLabel* m_page_label = nullptr;

  QTableWidget* m_mods_table = nullptr;
  QLabel* m_status_label = nullptr;

  QLabel* m_preview_image_label = nullptr;
  QLabel* m_detail_title = nullptr;
  QLabel* m_detail_meta = nullptr;
  QLabel* m_detail_stats = nullptr;
  QTextBrowser* m_detail_desc = nullptr;
  QComboBox* m_files_combo = nullptr;
  QProgressBar* m_download_progress = nullptr;
  QPushButton* m_install_btn = nullptr;
  QPushButton* m_open_browser_btn = nullptr;

  int m_current_page = 1;
  int m_selected_mod_index = -1;
  std::vector<ModItem> m_mods;

  // Installed Tab Widgets
  QCheckBox* m_chk_hires_textures = nullptr;
  QCheckBox* m_chk_cache_textures = nullptr;
  QCheckBox* m_chk_graphics_mods = nullptr;
  QTableWidget* m_installed_table = nullptr;
  QPushButton* m_btn_refresh_installed = nullptr;
  QPushButton* m_btn_toggle_installed = nullptr;
  QPushButton* m_btn_import_zip = nullptr;
  QPushButton* m_btn_open_mod_dir = nullptr;
  QPushButton* m_btn_open_textures_dir = nullptr;
  QPushButton* m_btn_delete_mod = nullptr;
  std::vector<InstalledModEntry> m_installed_mods;

  // Cheats Tab Widgets
  QComboBox* m_cheat_game_combo = nullptr;
  QLineEdit* m_cheat_game_id_edit = nullptr;
  QCheckBox* m_chk_enable_cheats = nullptr;
  QTableWidget* m_templates_table = nullptr;
  QPushButton* m_btn_use_template = nullptr;
  QListWidget* m_existing_codes_list = nullptr;
  QPushButton* m_btn_save_toggled_codes = nullptr;

  QLineEdit* m_cheat_name_edit = nullptr;
  QLineEdit* m_cheat_creator_edit = nullptr;
  QComboBox* m_cheat_type_combo = nullptr;
  QLineEdit* m_cheat_notes_edit = nullptr;
  QLineEdit* m_patch_addr_edit = nullptr;
  QLineEdit* m_patch_val_edit = nullptr;
  QComboBox* m_patch_type_combo = nullptr;
  QPushButton* m_btn_append_patch = nullptr;
  QPlainTextEdit* m_cheat_lines_edit = nullptr;
  QPushButton* m_btn_save_cheat = nullptr;
  QPushButton* m_btn_open_ini = nullptr;
};
