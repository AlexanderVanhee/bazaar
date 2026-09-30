/*
 * bz.vapi
 *
 * Copyright 2026 Eva M
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

[CCode (cheader_filename = "bz-addon-tile.h", type_id = "bz_addon_tile_get_type ()")]
public extern class Bz.AddonTile : GLib.Object {
    [CCode (has_construct_function = false)]
    protected AddonTile ();
}

[CCode (cheader_filename = "bz-addons-dialog.h", type_id = "bz_addons_dialog_get_type ()")]
public extern class Bz.AddonsDialog : GLib.Object {
    [CCode (has_construct_function = false)]
    protected AddonsDialog ();
}

[CCode (cheader_filename = "bz-age-rating-dialog.h", type_id = "bz_age_rating_dialog_get_type ()")]
public extern class Bz.AgeRatingDialog : GLib.Object {
    [CCode (has_construct_function = false)]
    protected AgeRatingDialog ();
}

[CCode (cheader_filename = "bz-all-apps-page.h", type_id = "bz_all_apps_page_get_type ()")]
public extern class Bz.AllAppsPage : GLib.Object {
    [CCode (has_construct_function = false)]
    protected AllAppsPage ();
}

[CCode (cheader_filename = "bz-app-permissions.h", type_id = "bz_app_permissions_get_type ()")]
public extern class Bz.AppPermissions : GLib.Object {
    [CCode (has_construct_function = false)]
    protected AppPermissions ();

    [CCode (cname = "bz_app_permissions_is_empty")]
    public bool is_empty ();
}

[CCode (cheader_filename = "bz-app-size-dialog.h", type_id = "bz_app_size_dialog_get_type ()")]
public extern class Bz.AppSizeDialog : GLib.Object {
    [CCode (has_construct_function = false)]
    protected AppSizeDialog ();
}

[CCode (cheader_filename = "bz-app-tile.h", type_id = "bz_app_tile_get_type ()")]
public extern class Bz.AppTile : GLib.Object {
    [CCode (has_construct_function = false)]
    protected AppTile ();
}

[CCode (cheader_filename = "bz-application-map-factory.h", type_id = "bz_application_map_factory_get_type ()")]
public extern class Bz.ApplicationMapFactory : GLib.Object {
    [CCode (has_construct_function = false)]
    protected ApplicationMapFactory ();

    public GLib.ListModel? generate (GLib.ListModel model);
}

[CCode (cheader_filename = "bz-application.h", type_id = "bz_application_get_type ()")]
public extern class Bz.Application : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Application ();
}

[CCode (cheader_filename = "bz-apps-page.h", type_id = "bz_apps_page_get_type ()")]
public extern class Bz.AppsPage : GLib.Object {
    [CCode (has_construct_function = false)]
    protected AppsPage ();
}

[CCode (cheader_filename = "bz-appstream-description-render.h", type_id = "bz_appstream_description_render_get_type ()")]
public extern class Bz.AppstreamDescriptionRender : GLib.Object {
    [CCode (has_construct_function = false)]
    protected AppstreamDescriptionRender ();
}

[CCode (cheader_filename = "bz-appstream-parser.h", type_id = "bz_appstream_parser_get_type ()")]
public extern class Bz.AppstreamParser : GLib.Object {
    [CCode (has_construct_function = false)]
    protected AppstreamParser ();
}

[CCode (cheader_filename = "bz-article-list-view.h", type_id = "bz_article_list_view_get_type ()")]
public extern class Bz.ArticleListView : GLib.Object {
    [CCode (has_construct_function = false)]
    protected ArticleListView ();
}

[CCode (cheader_filename = "bz-article-tile.h", type_id = "bz_article_tile_get_type ()")]
public extern class Bz.ArticleTile : GLib.Object {
    [CCode (has_construct_function = false)]
    protected ArticleTile ();
}

[CCode (cheader_filename = "bz-article.h", type_id = "bz_article_get_type ()")]
public extern class Bz.Article : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Article ();
}

[CCode (cheader_filename = "bz-aspect-picture.h", type_id = "bz_aspect_picture_get_type ()")]
public extern class Bz.AspectPicture : GLib.Object {
    [CCode (has_construct_function = false)]
    protected AspectPicture ();
}

[CCode (cheader_filename = "bz-async-texture.h", type_id = "bz_async_texture_get_type ()")]
public extern class Bz.AsyncTexture : GLib.Object, Gdk.Paintable {
    [CCode (has_construct_function = false)]
    protected AsyncTexture ();

    public Gdk.Texture? dup_texture ();
}

[CCode (cheader_filename = "bz-auth-state.h", type_id = "bz_auth_state_get_type ()")]
public extern class Bz.AuthState : GLib.Object {
    [CCode (has_construct_function = false)]
    protected AuthState ();
}

[CCode (cheader_filename = "bz-banner-view.h", type_id = "bz_banner_view_get_type ()")]
public extern class Bz.BannerView : GLib.Object {
    [CCode (has_construct_function = false)]
    protected BannerView ();
}

[CCode (cheader_filename = "bz-bundle-install-dialog.h", type_id = "bz_bundle_install_dialog_get_type ()")]
public extern class Bz.BundleInstallDialog : GLib.Object {
    [CCode (has_construct_function = false)]
    protected BundleInstallDialog ();
}

[CCode (cheader_filename = "bz-category-flags.h", type_id = "bz_category_flags_get_type ()")]
public extern class Bz.CategoryFlags : GLib.Object {
    [CCode (has_construct_function = false)]
    protected CategoryFlags ();
}

[CCode (cheader_filename = "bz-category-tile.h", type_id = "bz_category_tile_get_type ()")]
public extern class Bz.CategoryTile : GLib.Object {
    [CCode (has_construct_function = false)]
    protected CategoryTile ();
}

[CCode (cheader_filename = "bz-content-provider.h", type_id = "bz_content_provider_get_type ()")]
public extern class Bz.ContentProvider : GLib.Object {
    [CCode (has_construct_function = false)]
    protected ContentProvider ();
}

[CCode (cheader_filename = "bz-context-row.h")]
namespace Bz {
    [CCode (cname = "bz_context_row_new")]
    public unowned Adw.ActionRow context_row_new (string? icon_name, Bz.Importance importance, string title, string? subtitle);
}

[CCode (cheader_filename = "bz-context-tile.h", type_id = "bz_context_tile_get_type ()")]
public extern class Bz.ContextTile : GLib.Object {
    [CCode (has_construct_function = false)]
    protected ContextTile ();
}

[CCode (cheader_filename = "bz-curated-view.h", type_id = "bz_curated_view_get_type ()")]
public extern class Bz.CuratedView : GLib.Object {
    [CCode (has_construct_function = false)]
    protected CuratedView ();
}

[CCode (cheader_filename = "bz-data-graph.h", type_id = "bz_data_graph_get_type ()")]
public extern class Bz.DataGraph : GLib.Object {
    [CCode (has_construct_function = false)]
    protected DataGraph ();
}

[CCode (cheader_filename = "bz-developer-badge.h", type_id = "bz_developer_badge_get_type ()")]
public extern class Bz.DeveloperBadge : GLib.Object {
    [CCode (has_construct_function = false)]
    protected DeveloperBadge ();
}

[CCode (cheader_filename = "bz-donations-dialog.h", type_id = "bz_donations_dialog_get_type ()")]
public extern class Bz.DonationsDialog : GLib.Object {
    [CCode (has_construct_function = false)]
    protected DonationsDialog ();
}

[CCode (cheader_filename = "bz-download-worker.h", type_id = "bz_download_worker_get_type ()")]
public extern class Bz.DownloadWorker : GLib.Object {
    [CCode (has_construct_function = false)]
    protected DownloadWorker ();
}

[CCode (cheader_filename = "bz-dynamic-list-view.h", type_id = "bz_dynamic_list_view_get_type ()")]
public extern class Bz.DynamicListView : GLib.Object {
    [CCode (has_construct_function = false)]
    protected DynamicListView ();

    public GLib.ListModel? model { get; set; }
}

[CCode (cheader_filename = "bz-entry-cache-manager.h", type_id = "bz_entry_cache_manager_get_type ()")]
public extern class Bz.EntryCacheManager : GLib.Object {
    [CCode (has_construct_function = false)]
    protected EntryCacheManager ();
}

[CCode (cheader_filename = "bz-entry-group-util.h", type_id = "bz_entry_group_util_get_type ()")]
public extern class Bz.EntryGroupUtil : GLib.Object {
    [CCode (has_construct_function = false)]
    protected EntryGroupUtil ();
}

[CCode (cheader_filename = "bz-entry-group.h", type_id = "bz_entry_group_get_type ()")]
public extern class Bz.EntryGroup : GLib.Object {
    [CCode (has_construct_function = false)]
    protected EntryGroup ();

    [CCode (cname = "bz_entry_group_get_unique_id_is_eol")]
    public bool get_unique_id_is_eol (string unique_id);

    public Dex.Future dup_all_into_store ();
    public string? dup_ui_entry_id ();
}

[CCode (cheader_filename = "bz-entry-inspector.h", type_id = "bz_entry_inspector_get_type ()")]
public extern class Bz.EntryInspector : GLib.Object {
    [CCode (has_construct_function = false)]
    protected EntryInspector ();
}

[Flags]
[CCode (cheader_filename = "bz-entry.h", cname = "BzEntryKind", cprefix = "BZ_ENTRY_KIND_", has_type_id = false)]
public enum Bz.EntryKind {
    APPLICATION,
    RUNTIME,
    ADDON,
}

[CCode (cheader_filename = "bz-entry.h", type_id = "bz_entry_get_type ()")]
public extern class Bz.Entry : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Entry ();

    public string? unique_id { get; set; }

    public string? title { get; }
    public string? developer { get; }
    public Gdk.Paintable? icon_paintable { get; }
    [NoAccessorMethod]
    public GLib.ListModel? version_history { owned get; }
    public string? installed_version { get; }
    public Bz.Repository? get_repository (GLib.ListModel repos);

    [CCode (cname = "bz_entry_is_of_kinds")]
    public bool is_of_kinds (Bz.EntryKind kinds);
}

[CCode (cheader_filename = "bz-error-dialog.h", type_id = "bz_error_dialog_get_type ()")]
public extern class Bz.ErrorDialog : GLib.Object {
    [CCode (has_construct_function = false)]
    protected ErrorDialog ();
}

[CCode (cheader_filename = "bz-fading-clamp.h", type_id = "bz_fading_clamp_get_type ()")]
public extern class Bz.FadingClamp : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FadingClamp ();
}

[CCode (cheader_filename = "bz-favorite-button.h", type_id = "bz_favorite_button_get_type ()")]
public extern class Bz.FavoriteButton : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FavoriteButton ();
}

[CCode (cheader_filename = "bz-favorites-page.h", type_id = "bz_favorites_page_get_type ()")]
public extern class Bz.FavoritesPage : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FavoritesPage ();
}

[CCode (cheader_filename = "bz-favorites-tile.h", type_id = "bz_favorites_tile_get_type ()")]
public extern class Bz.FavoritesTile : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FavoritesTile ();
}

[CCode (cheader_filename = "bz-featured-carousel-view.h", type_id = "bz_featured_carousel_view_get_type ()")]
public extern class Bz.FeaturedCarouselView : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FeaturedCarouselView ();
}

[CCode (cheader_filename = "bz-featured-carousel.h", type_id = "bz_featured_carousel_get_type ()")]
public extern class Bz.FeaturedCarousel : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FeaturedCarousel ();
}

[CCode (cheader_filename = "bz-featured-tile.h", type_id = "bz_featured_tile_get_type ()")]
public extern class Bz.FeaturedTile : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FeaturedTile ();
}

[CCode (cheader_filename = "bz-flathub-category-section.h", type_id = "bz_flathub_category_section_get_type ()")]
public extern class Bz.FlathubCategorySection : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FlathubCategorySection ();
}

[CCode (cheader_filename = "bz-flathub-category.h", type_id = "bz_flathub_category_get_type ()")]
public extern class Bz.FlathubCategory : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FlathubCategory ();
}

[CCode (cheader_filename = "bz-flathub-page.h", type_id = "bz_flathub_page_get_type ()")]
public extern class Bz.FlathubPage : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FlathubPage ();
}

[CCode (cheader_filename = "bz-flathub-state.h", type_id = "bz_flathub_state_get_type ()")]
public extern class Bz.FlathubState : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FlathubState ();
}

[CCode (cheader_filename = "bz-flatpak-entry.h", type_id = "bz_flatpak_entry_get_type ()")]
public extern class Bz.FlatpakEntry : Bz.Entry {
    [CCode (has_construct_function = false)]
    protected FlatpakEntry ();

    public string? flatpak_version { get; set; }
}

[CCode (cheader_filename = "bz-flatpak-instance.h", type_id = "bz_flatpak_instance_get_type ()")]
public extern class Bz.FlatpakInstance : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FlatpakInstance ();
}

[CCode (cheader_filename = "bz-flatpak-private.h", type_id = "bz_flatpak_private_get_type ()")]
public extern class Bz.FlatpakPrivate : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FlatpakPrivate ();
}

[CCode (cheader_filename = "bz-full-view.h", type_id = "bz_full_view_get_type ()")]
public extern class Bz.FullView : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FullView ();
}

[CCode (cheader_filename = "bz-group-tile-css-watcher.h", type_id = "bz_group_tile_css_watcher_get_type ()")]
public extern class Bz.GroupTileCssWatcher : GLib.Object {
    [CCode (has_construct_function = false)]
    protected GroupTileCssWatcher ();
}

[CCode (cheader_filename = "bz-hardware-support-dialog.h", type_id = "bz_hardware_support_dialog_get_type ()")]
public extern class Bz.HardwareSupportDialog : GLib.Object {
    [CCode (has_construct_function = false)]
    protected HardwareSupportDialog ();
}

[CCode (cheader_filename = "bz-inspector.h", type_id = "bz_inspector_get_type ()")]
public extern class Bz.Inspector : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Inspector ();
}

[CCode (cheader_filename = "bz-install-controls.h", type_id = "bz_install_controls_get_type ()")]
public extern class Bz.InstallControls : GLib.Object {
    [CCode (has_construct_function = false)]
    protected InstallControls ();
}

[CCode (cheader_filename = "bz-installed-tile.h", type_id = "bz_installed_tile_get_type ()")]
public extern class Bz.InstalledTile : GLib.Object {
    [CCode (has_construct_function = false)]
    protected InstalledTile ();
}

[CCode (cheader_filename = "bz-lazy-wdgt.h", type_id = "bz_lazy_wdgt_get_type ()")]
public extern class Bz.LazyWdgt : GLib.Object {
    [CCode (has_construct_function = false)]
    protected LazyWdgt ();
}

[CCode (cheader_filename = "bz-library-page.h", type_id = "bz_library_page_get_type ()")]
public extern class Bz.LibraryPage : GLib.Object {
    [CCode (has_construct_function = false)]
    protected LibraryPage ();
}

[CCode (cheader_filename = "bz-license-dialog.h", type_id = "bz_license_dialog_get_type ()")]
public extern class Bz.LicenseDialog : GLib.Object {
    [CCode (has_construct_function = false)]
    protected LicenseDialog ();
}

[CCode (cheader_filename = "bz-list-tile.h", type_id = "bz_list_tile_get_type ()")]
public extern class Bz.ListTile : GLib.Object {
    [CCode (has_construct_function = false)]
    protected ListTile ();
}

[CCode (cheader_filename = "bz-login-page.h", type_id = "bz_login_page_get_type ()")]
public extern class Bz.LoginPage : GLib.Object {
    [CCode (has_construct_function = false)]
    protected LoginPage ();
}

[CCode (cheader_filename = "bz-lozenge.h", type_id = "bz_lozenge_get_type ()")]
public extern class Bz.Lozenge : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Lozenge ();
}

[CCode (cheader_filename = "bz-malcontent-service.h", type_id = "bz_malcontent_service_get_type ()")]
public extern class Bz.MalcontentService : GLib.Object {
    [CCode (has_construct_function = false)]
    protected MalcontentService ();
}

[CCode (cheader_filename = "bz-metainfo-preview.h", type_id = "bz_metainfo_preview_get_type ()")]
public extern class Bz.MetainfoPreview : GLib.Object {
    [CCode (has_construct_function = false)]
    protected MetainfoPreview ();
}

[CCode (cheader_filename = "bz-newline-parser.h", type_id = "bz_newline_parser_get_type ()")]
public extern class Bz.NewlineParser : GLib.Object {
    [CCode (has_construct_function = false)]
    protected NewlineParser ();
}

[CCode (cheader_filename = "bz-parser.h", type_id = "bz_parser_get_type ()")]
public extern class Bz.Parser : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Parser ();
}

[CCode (cheader_filename = "bz-preferences-dialog.h", type_id = "bz_preferences_dialog_get_type ()")]
public extern class Bz.PreferencesDialog : GLib.Object {
    [CCode (has_construct_function = false)]
    protected PreferencesDialog ();
}

[CCode (cheader_filename = "bz-progress-bar.h", type_id = "bz_progress_bar_get_type ()")]
public extern class Bz.ProgressBar : GLib.Object {
    [CCode (has_construct_function = false)]
    protected ProgressBar ();
}

[CCode (cheader_filename = "bz-releases-list.h", type_id = "bz_releases_list_get_type ()")]
public extern class Bz.ReleasesList : GLib.Object {
    [CCode (has_construct_function = false)]
    protected ReleasesList ();

    [CCode (cname = "bz_releases_dialog_new")]
    public static unowned Gtk.Widget dialog_new (GLib.ListModel? version_history, GLib.ListModel? installed_versions);
}

[CCode (cheader_filename = "bz-result.h", type_id = "bz_result_get_type ()")]
public extern class Bz.Result : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Result ();
}

[CCode (cheader_filename = "bz-rich-app-tile.h", type_id = "bz_rich_app_tile_get_type ()")]
public extern class Bz.RichAppTile : GLib.Object {
    [CCode (has_construct_function = false)]
    protected RichAppTile ();
}

[CCode (cheader_filename = "bz-rounded-picture.h", type_id = "bz_rounded_picture_get_type ()")]
public extern class Bz.RoundedPicture : GLib.Object {
    [CCode (has_construct_function = false)]
    protected RoundedPicture ();
}

[CCode (cheader_filename = "bz-row-view.h", type_id = "bz_row_view_get_type ()")]
public extern class Bz.RowView : GLib.Object {
    [CCode (has_construct_function = false)]
    protected RowView ();
}

[CCode (cheader_filename = "bz-safety-dialog.h", type_id = "bz_safety_dialog_get_type ()")]
public extern class Bz.SafetyDialog : GLib.Object {
    [CCode (has_construct_function = false)]
    protected SafetyDialog ();
}

[CCode (cheader_filename = "bz-screenshot.h", type_id = "bz_screenshot_get_type ()")]
public extern class Bz.Screenshot : Gtk.Widget {
    [CCode (has_construct_function = false, type = "GtkWidget*")]
    public Screenshot ();

    public Gdk.Paintable? paintable { get; set; }
    public bool rounded_corners { get; set; }
    public Gsk.ScalingFilter filter { get; set; }
}

[CCode (cheader_filename = "bz-screenshots-carousel.h", type_id = "bz_screenshots_carousel_get_type ()")]
public extern class Bz.ScreenshotsCarousel : GLib.Object {
    [CCode (has_construct_function = false)]
    protected ScreenshotsCarousel ();
}

[CCode (cheader_filename = "bz-search-bar.h", type_id = "bz_search_bar_get_type ()")]
public extern class Bz.SearchBar : GLib.Object {
    [CCode (has_construct_function = false)]
    protected SearchBar ();
}

[CCode (cheader_filename = "bz-search-engine.h", type_id = "bz_search_engine_get_type ()")]
public extern class Bz.SearchEngine : GLib.Object {
    [CCode (has_construct_function = false)]
    protected SearchEngine ();
}

[CCode (cheader_filename = "bz-search-filter-popover.h", type_id = "bz_search_filter_popover_get_type ()")]
public extern class Bz.SearchFilterPopover : GLib.Object {
    [CCode (has_construct_function = false)]
    protected SearchFilterPopover ();
}

[CCode (cheader_filename = "bz-search-page.h", type_id = "bz_search_page_get_type ()")]
public extern class Bz.SearchPage : GLib.Object {
    [CCode (has_construct_function = false)]
    protected SearchPage ();
}

[CCode (cheader_filename = "bz-search-pill-list.h", type_id = "bz_search_pill_list_get_type ()")]
public extern class Bz.SearchPillList : GLib.Object {
    [CCode (has_construct_function = false)]
    protected SearchPillList ();
}

[CCode (cheader_filename = "bz-section-view.h", type_id = "bz_section_view_get_type ()")]
public extern class Bz.SectionView : GLib.Object {
    [CCode (has_construct_function = false)]
    protected SectionView ();
}

[CCode (cheader_filename = "bz-serializable.h", type_id = "bz_serializable_get_type ()")]
public extern class Bz.Serializable : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Serializable ();
}

[CCode (cheader_filename = "bz-share-list.h", type_id = "bz_share_list_get_type ()")]
public extern class Bz.ShareList : GLib.Object {
    [CCode (has_construct_function = false)]
    protected ShareList ();
}

[CCode (cheader_filename = "bz-stats-dialog.h", type_id = "bz_stats_dialog_get_type ()")]
public extern class Bz.StatsDialog : GLib.Object {
    [CCode (has_construct_function = false)]
    protected StatsDialog ();
}

[CCode (cheader_filename = "bz-subcategory-list.h", type_id = "bz_subcategory_list_get_type ()")]
public extern class Bz.SubcategoryList : GLib.Object {
    [CCode (has_construct_function = false)]
    protected SubcategoryList ();
}

[CCode (cheader_filename = "bz-transact-icon.h", type_id = "bz_transact_icon_get_type ()")]
public extern class Bz.TransactIcon : GLib.Object {
    [CCode (has_construct_function = false)]
    protected TransactIcon ();
}

[CCode (cheader_filename = "bz-transaction-dialog.h", type_id = "bz_transaction_dialog_get_type ()")]
public extern class Bz.TransactionDialog : GLib.Object {
    [CCode (has_construct_function = false)]
    protected TransactionDialog ();
}

[CCode (cheader_filename = "bz-transaction-list-dialog.h", type_id = "bz_transaction_list_dialog_get_type ()")]
public extern class Bz.TransactionListDialog : GLib.Object {
    [CCode (has_construct_function = false)]
    protected TransactionListDialog ();
}

[CCode (cheader_filename = "bz-transaction-manager.h", type_id = "bz_transaction_manager_get_type ()")]
public extern class Bz.TransactionManager : GLib.Object {
    [CCode (has_construct_function = false)]
    protected TransactionManager ();
}

[CCode (cheader_filename = "bz-transaction-tile.h", type_id = "bz_transaction_tile_get_type ()")]
public extern class Bz.TransactionTile : GLib.Object {
    [CCode (has_construct_function = false)]
    protected TransactionTile ();
}

[CCode (cheader_filename = "bz-transaction.h", type_id = "bz_transaction_get_type ()")]
public extern class Bz.Transaction : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Transaction ();
}

[CCode (cheader_filename = "bz-update-info.h", type_id = "bz_update_permission_info_get_type ()")]
public extern class Bz.UpdatePermissionInfo : GLib.Object {
    [CCode (has_construct_function = false)]
    protected UpdatePermissionInfo ();

    public Bz.Entry? entry { get; set; }
    public Bz.AppPermissions? additional_permissions { get; set;}
}

[CCode (cheader_filename = "bz-update-history-dialog.h", type_id = "bz_update_history_dialog_get_type ()")]
public extern class Bz.UpdateHistoryDialog : GLib.Object {
    [CCode (has_construct_function = false)]
    protected UpdateHistoryDialog ();
}

[CCode (cheader_filename = "bz-user-data-page.h", type_id = "bz_user_data_page_get_type ()")]
public extern class Bz.UserDataPage : GLib.Object {
    [CCode (has_construct_function = false)]
    protected UserDataPage ();
}

[CCode (cheader_filename = "bz-user-data-tile.h", type_id = "bz_user_data_tile_get_type ()")]
public extern class Bz.UserDataTile : GLib.Object {
    [CCode (has_construct_function = false)]
    protected UserDataTile ();
}

[CCode (cheader_filename = "bz-window.h", type_id = "bz_window_get_type ()")]
public extern class Bz.Window : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Window ();
}

[CCode (cheader_filename = "bz-world-map-parser.h", type_id = "bz_world_map_parser_get_type ()")]
public extern class Bz.WorldMapParser : GLib.Object {
    [CCode (has_construct_function = false)]
    protected WorldMapParser ();
}

[CCode (cheader_filename = "bz-world-map.h", type_id = "bz_world_map_get_type ()")]
public extern class Bz.WorldMap : GLib.Object {
    [CCode (has_construct_function = false)]
    protected WorldMap ();
}

[CCode (cheader_filename = "bz-yaml-parser.h", type_id = "bz_yaml_parser_get_type ()")]
public extern class Bz.YamlParser : GLib.Object {
    [CCode (has_construct_function = false)]
    protected YamlParser ();
}

[CCode (cheader_filename = "bz-zoom.h", type_id = "bz_zoom_get_type ()")]
public extern class Bz.Zoom : Gtk.Widget, Gtk.Scrollable {
    [CCode (has_construct_function = false, type = "GtkWidget*")]
    public Zoom ();

    public Gtk.Widget? child { get; set; }
    public double zoom_level { get; set; }
    public double min_zoom { get; set; }
    public double max_zoom { get; set; }

    public bool is_transformed ();
    public Gsk.Transform? get_child_transform ();
    public bool compute_child_bounds (Gtk.Widget target, out Graphene.Rect bounds);
    public void zoom_in ();
    public void zoom_out ();
    public void reset ();
    public void fit_to_window ();
}

[CCode (cheader_filename = "bz-backend.h", type_id = "bz_backend_get_type ()")]
public extern interface Bz.Backend : GLib.Object {}


/* gen_gobject.sh */

[CCode (cheader_filename = "bz-age-rating-attribute.h", type_id = "bz_age_rating_attribute_get_type ()")]
public extern class Bz.AgeRatingAttribute : GLib.Object {
    [CCode (has_construct_function = false)]
    protected AgeRatingAttribute ();
}

[CCode (cheader_filename = "bz-backend-notification.h", type_id = "bz_backend_notification_get_type ()")]
public extern class Bz.BackendNotification : GLib.Object {
    [CCode (has_construct_function = false)]
    protected BackendNotification ();
}

[CCode (cheader_filename = "bz-backend-transaction-op-payload.h", type_id = "bz_backend_transaction_op_payload_get_type ()")]
public extern class Bz.BackendTransactionOpPayload : GLib.Object {
    [CCode (has_construct_function = false)]
    protected BackendTransactionOpPayload ();
}

[CCode (cheader_filename = "bz-backend-transaction-op-progress-payload.h", type_id = "bz_backend_transaction_op_progress_payload_get_type ()")]
public extern class Bz.BackendTransactionOpProgressPayload : GLib.Object {
    [CCode (has_construct_function = false)]
    protected BackendTransactionOpProgressPayload ();
}

[CCode (cheader_filename = "bz-blocklist.h", type_id = "bz_blocklist_get_type ()")]
public extern class Bz.Blocklist : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Blocklist ();
}

[CCode (cheader_filename = "bz-blocklist-condition.h", type_id = "bz_blocklist_condition_get_type ()")]
public extern class Bz.BlocklistCondition : GLib.Object {
    [CCode (has_construct_function = false)]
    protected BlocklistCondition ();
}

[CCode (cheader_filename = "bz-blocklist-condition-match-envvar.h", type_id = "bz_blocklist_condition_match_envvar_get_type ()")]
public extern class Bz.BlocklistConditionMatchEnvvar : GLib.Object {
    [CCode (has_construct_function = false)]
    protected BlocklistConditionMatchEnvvar ();
}

[CCode (cheader_filename = "bz-blocklist-condition-match-locale.h", type_id = "bz_blocklist_condition_match_locale_get_type ()")]
public extern class Bz.BlocklistConditionMatchLocale : GLib.Object {
    [CCode (has_construct_function = false)]
    protected BlocklistConditionMatchLocale ();
}

[CCode (cheader_filename = "bz-bulk-install-dialog-result.h", type_id = "bz_bulk_install_dialog_result_get_type ()")]
public extern class Bz.BulkInstallDialogResult : GLib.Object {
    [CCode (has_construct_function = false)]
    protected BulkInstallDialogResult ();
}

[CCode (cheader_filename = "bz-country.h", type_id = "bz_country_get_type ()")]
public extern class Bz.Country : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Country ();
}

[CCode (cheader_filename = "bz-country-data-point.h", type_id = "bz_country_data_point_get_type ()")]
public extern class Bz.CountryDataPoint : GLib.Object {
    [CCode (has_construct_function = false)]
    protected CountryDataPoint ();
}

[CCode (cheader_filename = "bz-curated-appids-info.h", type_id = "bz_curated_appids_info_get_type ()")]
public extern class Bz.CuratedAppidsInfo : GLib.Object {
    [CCode (has_construct_function = false)]
    protected CuratedAppidsInfo ();
}

[CCode (cheader_filename = "bz-curated-article.h", type_id = "bz_curated_article_get_type ()")]
public extern class Bz.CuratedArticle : GLib.Object {
    [CCode (has_construct_function = false)]
    protected CuratedArticle ();
}

[CCode (cheader_filename = "bz-curated-articles-info.h", type_id = "bz_curated_articles_info_get_type ()")]
public extern class Bz.CuratedArticlesInfo : GLib.Object {
    [CCode (has_construct_function = false)]
    protected CuratedArticlesInfo ();
}

[CCode (cheader_filename = "bz-curated-banner.h", type_id = "bz_curated_banner_get_type ()")]
public extern class Bz.CuratedBanner : GLib.Object {
    [CCode (has_construct_function = false)]
    protected CuratedBanner ();
}

[CCode (cheader_filename = "bz-curated-featured-carousel.h", type_id = "bz_curated_featured_carousel_get_type ()")]
public extern class Bz.CuratedFeaturedCarousel : GLib.Object {
    [CCode (has_construct_function = false)]
    protected CuratedFeaturedCarousel ();
}

[CCode (cheader_filename = "bz-curated-image-info.h", type_id = "bz_curated_image_info_get_type ()")]
public extern class Bz.CuratedImageInfo : GLib.Object {
    [CCode (has_construct_function = false)]
    protected CuratedImageInfo ();
}

[CCode (cheader_filename = "bz-curated-markdown-info.h", type_id = "bz_curated_markdown_info_get_type ()")]
public extern class Bz.CuratedMarkdownInfo : GLib.Object {
    [CCode (has_construct_function = false)]
    protected CuratedMarkdownInfo ();
}

[CCode (cheader_filename = "bz-curated-row.h", type_id = "bz_curated_row_get_type ()")]
public extern class Bz.CuratedRow : GLib.Object {
    [CCode (has_construct_function = false)]
    protected CuratedRow ();
}

[CCode (cheader_filename = "bz-curated-section.h", type_id = "bz_curated_section_get_type ()")]
public extern class Bz.CuratedSection : GLib.Object {
    [CCode (has_construct_function = false)]
    protected CuratedSection ();
}

[CCode (cheader_filename = "bz-data-point.h", type_id = "bz_data_point_get_type ()")]
public extern class Bz.DataPoint : GLib.Object {
    [CCode (has_construct_function = false)]
    protected DataPoint ();
}

[CCode (cheader_filename = "bz-exponential-function.h", type_id = "bz_exponential_function_get_type ()")]
public extern class Bz.ExponentialFunction : GLib.Object {
    [CCode (has_construct_function = false)]
    protected ExponentialFunction ();
}

[CCode (cheader_filename = "bz-finished-search-query.h", type_id = "bz_finished_search_query_get_type ()")]
public extern class Bz.FinishedSearchQuery : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FinishedSearchQuery ();
}

[CCode (cheader_filename = "bz-flathub-auth-provider.h", type_id = "bz_flathub_auth_provider_get_type ()")]
public extern class Bz.FlathubAuthProvider : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FlathubAuthProvider ();
}

[CCode (cheader_filename = "bz-flathub-curated-selection.h", type_id = "bz_flathub_curated_selection_get_type ()")]
public extern class Bz.FlathubCuratedSelection : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FlathubCuratedSelection ();

    public string? theme_key { get; set;}
    public string? slot { get; set;}
    public Gtk.StringList? apps { get; set;}
    public Bz.ApplicationMapFactory? map_factory { get; set;}
}

[CCode (cheader_filename = "bz-flathub-sub-category.h", type_id = "bz_flathub_sub_category_get_type ()")]
public extern class Bz.FlathubSubCategory : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FlathubSubCategory ();
}

[CCode (cheader_filename = "bz-flatpak-bundle-result.h", type_id = "bz_flatpak_bundle_result_get_type ()")]
public extern class Bz.FlatpakBundleResult : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FlatpakBundleResult ();
}

[CCode (cheader_filename = "bz-flatpak-repo.h", type_id = "bz_flatpak_repo_get_type ()")]
public extern class Bz.FlatpakRepo : GLib.Object {
    [CCode (has_construct_function = false)]
    protected FlatpakRepo ();
}

[CCode (cheader_filename = "bz-hash-table-object.h", type_id = "bz_hash_table_object_get_type ()")]
public extern class Bz.HashTableObject : GLib.Object {
    [CCode (has_construct_function = false)]
    protected HashTableObject ();
}

[CCode (cheader_filename = "bz-hook.h", type_id = "bz_hook_get_type ()")]
public extern class Bz.Hook : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Hook ();
}

[CCode (cheader_filename = "bz-hook-dialog.h", type_id = "bz_hook_dialog_get_type ()")]
public extern class Bz.HookDialog : GLib.Object {
    [CCode (has_construct_function = false)]
    protected HookDialog ();
}

[CCode (cheader_filename = "bz-hook-dialog-option.h", type_id = "bz_hook_dialog_option_get_type ()")]
public extern class Bz.HookDialogOption : GLib.Object {
    [CCode (has_construct_function = false)]
    protected HookDialogOption ();
}

[CCode (cheader_filename = "bz-internal-config.h", type_id = "bz_internal_config_get_type ()")]
public extern class Bz.InternalConfig : GLib.Object {
    [CCode (has_construct_function = false)]
    protected InternalConfig ();
}

[CCode (cheader_filename = "bz-linear-function.h", type_id = "bz_linear_function_get_type ()")]
public extern class Bz.LinearFunction : GLib.Object {
    [CCode (has_construct_function = false)]
    protected LinearFunction ();
}

[CCode (cheader_filename = "bz-main-config.h", type_id = "bz_main_config_get_type ()")]
public extern class Bz.MainConfig : GLib.Object {
    [CCode (has_construct_function = false)]
    protected MainConfig ();
}

[CCode (cheader_filename = "bz-pride-flag-config.h", type_id = "bz_pride_flag_config_get_type ()")]
public extern class Bz.PrideFlagConfig : GLib.Object {
    [CCode (has_construct_function = false)]
    protected PrideFlagConfig ();
}

[CCode (cheader_filename = "bz-pride-flag-spec.h", type_id = "bz_pride_flag_spec_get_type ()")]
public extern class Bz.PrideFlagSpec : GLib.Object {
    [CCode (has_construct_function = false)]
    protected PrideFlagSpec ();
}

[CCode (cheader_filename = "bz-pride-flag-stripe-spec.h", type_id = "bz_pride_flag_stripe_spec_get_type ()")]
public extern class Bz.PrideFlagStripeSpec : GLib.Object {
    [CCode (has_construct_function = false)]
    protected PrideFlagStripeSpec ();
}

[CCode (cheader_filename = "bz-release.h", type_id = "bz_release_get_type ()")]
public extern class Bz.Release : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Release ();

    public string? version { get; set; }
}

[CCode (cheader_filename = "bz-repository.h", type_id = "bz_repository_get_type ()")]
public extern class Bz.Repository : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Repository ();

    public bool is_user { get; set; }
    public bool is_beta { get; set; }
    public string? title { get; set; }
    public string? url { get; set; }
    public string? name { get; set; }
}

[CCode (cheader_filename = "bz-root-blocklist.h", type_id = "bz_root_blocklist_get_type ()")]
public extern class Bz.RootBlocklist : GLib.Object {
    [CCode (has_construct_function = false)]
    protected RootBlocklist ();
}

[CCode (cheader_filename = "bz-root-curated-config.h", type_id = "bz_root_curated_config_get_type ()")]
public extern class Bz.RootCuratedConfig : GLib.Object {
    [CCode (has_construct_function = false)]
    protected RootCuratedConfig ();
}

[CCode (cheader_filename = "safety-calculator.h", cname = "BzImportance", cprefix = "BZ_IMPORTANCE_", has_type_id = false)]
public enum Bz.Importance {
    UNIMPORTANT,
    NEUTRAL,
    INFORMATION,
    WARNING,
    IMPORTANT,
}

[CCode (cheader_filename = "safety-calculator.h")]
namespace Bz.SafetyCalculator {
    [CCode (cname = "bz_safety_calculator_analyze_permissions")]
    public static GLib.ListModel analyze_permissions (Bz.AppPermissions? permissions, bool include_no_access_rows);

    [CCode (cname = "bz_safety_calculator_analyze_entry")]
    public GLib.ListModel analyze_entry (Bz.Entry entry);

    [CCode (cname = "bz_safety_calculator_calculate_rating")]
    public Bz.Importance calculate_rating (Bz.Entry entry);
}

[CCode (cheader_filename = "bz-safety-row.h", type_id = "bz_safety_row_get_type ()")]
public extern class Bz.SafetyRow : GLib.Object {
    [CCode (has_construct_function = false)]
    protected SafetyRow ();

    public Bz.Importance importance { get; set;}
    public string? icon_name { get; set;}
    public string? title { get; set;}
    public string? subtitle { get; set;}
}

[CCode (cheader_filename = "bz-search-bias.h", type_id = "bz_search_bias_get_type ()")]
public extern class Bz.SearchBias : GLib.Object {
    [CCode (has_construct_function = false)]
    protected SearchBias ();
}

[CCode (cheader_filename = "bz-search-result.h", type_id = "bz_search_result_get_type ()")]
public extern class Bz.SearchResult : GLib.Object {
    [CCode (has_construct_function = false)]
    protected SearchResult ();
}

[CCode (cheader_filename = "bz-size-result.h", type_id = "bz_size_result_get_type ()")]
public extern class Bz.SizeResult : GLib.Object {
    [CCode (has_construct_function = false)]
    protected SizeResult ();
}

[CCode (cheader_filename = "bz-state-info.h", type_id = "bz_state_info_get_type ()")]
public extern class Bz.StateInfo : GLib.Object {
    [CCode (has_construct_function = false)]
    protected StateInfo ();

    [CCode (cname = "bz_state_info_get_default", cheader_filename = "bz-application.h")]
    public static unowned Bz.StateInfo get_default ();

    public GLib.ListModel? repositories { get; }
    public GLib.ListModel? available_updates { get; set; }
}

[CCode (cheader_filename = "bz-transact-icon-info.h", type_id = "bz_transact_icon_info_get_type ()")]
public extern class Bz.TransactIconInfo : GLib.Object {
    [CCode (has_construct_function = false)]
    protected TransactIconInfo ();
}

[CCode (cheader_filename = "bz-transaction-dialog-result.h", type_id = "bz_transaction_dialog_result_get_type ()")]
public extern class Bz.TransactionDialogResult : GLib.Object {
    [CCode (has_construct_function = false)]
    protected TransactionDialogResult ();
}

[CCode (cheader_filename = "bz-transaction-entry-tracker.h", type_id = "bz_transaction_entry_tracker_get_type ()")]
public extern class Bz.TransactionEntryTracker : GLib.Object {
    [CCode (has_construct_function = false)]
    protected TransactionEntryTracker ();
}

[CCode (cheader_filename = "bz-transaction-task.h", type_id = "bz_transaction_task_get_type ()")]
public extern class Bz.TransactionTask : GLib.Object {
    [CCode (has_construct_function = false)]
    protected TransactionTask ();
}

[CCode (cheader_filename = "bz-update-history-data-point.h", type_id = "bz_update_history_data_point_get_type ()")]
public extern class Bz.UpdateHistoryDataPoint : GLib.Object {
    [CCode (has_construct_function = false)]
    protected UpdateHistoryDataPoint ();
}

[CCode (cheader_filename = "bz-url.h", type_id = "bz_url_get_type ()")]
public extern class Bz.Url : GLib.Object {
    [CCode (has_construct_function = false)]
    protected Url ();
}

[CCode (cheader_filename = "bz-verification-status.h", type_id = "bz_verification_status_get_type ()")]
public extern class Bz.VerificationStatus : GLib.Object {
    [CCode (has_construct_function = false)]
    protected VerificationStatus ();
}
