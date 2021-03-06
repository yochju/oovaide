/*
 * OptionsDialog.cpp
 *
 *  Created on: Jul 11, 2013
 *  \copyright 2013 DCBlaha.  Distributed under the GPL.
 */

#include "OptionsDialog.h"
#include "Options.h"
#include "Gui.h"
#include "OovError.h"
#include "BuildVariablesDialog.h"
#include <gtk/gtk.h>
#include <memory>       // For unique_ptr
#include <algorithm>
#include "Project.h"
// Prevent "error: 'off64_t' does not name a type"
#define __NO_MINGW_LFS 1
// Prevent "error: 'off_t' has not been declared"
#define off_t _off_t
#include <unistd.h>             // for unlink

static OptionsDialog *gOptionsDlg;


class Option
    {
    public:
        Option(OovStringRef const optionName, OovStringRef const widgetName):
            mOptionName(optionName), mWidgetName(widgetName)
            {}
        virtual ~Option();
        virtual void optionToScreen(NameValueFile const &file) const = 0;
        virtual void screenToOption(NameValueFile &file) const = 0;
    protected:
        OovString mOptionName;
        OovString mWidgetName;
    };

Option::~Option()
    {}

class EntryOption:public Option
    {
    public:
        EntryOption(OovStringRef const optionName, OovStringRef const widgetName):
            Option(optionName, widgetName)
            {}
        virtual ~EntryOption();
        virtual void optionToScreen(NameValueFile const &file) const override;
        virtual void screenToOption(NameValueFile &file) const override;
    };

EntryOption::~EntryOption()
    {
    }

void EntryOption::optionToScreen(NameValueFile const &file) const
    {
    GtkEntry *entry = GTK_ENTRY(Builder::getBuilder()->getWidget(mWidgetName));
    Gui::setText(entry, file.getValue(mOptionName));
    }

void EntryOption::screenToOption(NameValueFile &file) const
    {
    GtkEntry *editPath = GTK_ENTRY(Builder::getBuilder()->getWidget(mWidgetName));
    file.setNameValue(mOptionName, Gui::getText(editPath));
    }

class CheckOption:public Option
    {
    public:
        CheckOption(char const * const optionName, char const * const widgetName):
            Option(optionName, widgetName)
            {}
        virtual ~CheckOption();
        virtual void optionToScreen(NameValueFile const &file) const override;
        virtual void screenToOption(NameValueFile &file) const override;
    };

CheckOption::~CheckOption()
    {
    }

void CheckOption::optionToScreen(NameValueFile const &file) const
    {
    bool active = file.getValueBool(mOptionName);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
            Builder::getBuilder()->getWidget(mWidgetName)), active);
    }

void CheckOption::screenToOption(NameValueFile &file) const
    {
    bool active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
            Builder::getBuilder()->getWidget(mWidgetName)));
    file.setNameValueBool(mOptionName, active);
    }

class TextViewBuildOption:public Option
    {
    public:
        /// The buildVar value is never retrieved or filled.
        TextViewBuildOption(OovStringRef optionName, OovStringRef const widgetName):
            Option(optionName, widgetName)
            {}
        virtual ~TextViewBuildOption();
        virtual void optionToScreen(NameValueFile const &file) const override;
        virtual void screenToOption(NameValueFile &file) const override;
    };

TextViewBuildOption::~TextViewBuildOption()
    {
    }

void TextViewBuildOption::optionToScreen(NameValueFile const &file) const
    {
    CompoundValue cppArgs;
    cppArgs.parseString(file.getValue(mOptionName));
    GtkTextView *view = GTK_TEXT_VIEW(Builder::getBuilder()->getWidget(mWidgetName));
    Gui::clear(view);
    std::string str = cppArgs.getAsString('\n');
    Gui::appendText(view, str);
    }

void TextViewBuildOption::screenToOption(NameValueFile &file) const
    {
    GtkTextView *view = GTK_TEXT_VIEW(Builder::getBuilder()->getWidget(mWidgetName));
    std::string str = Gui::getText(view);
    CompoundValue cppArgs;
    cppArgs.parseString(str, '\n');
    std::string newCppArgsStr = cppArgs.getAsString();
    file.setNameValue(mOptionName, newCppArgsStr);
    }

/////////////////////////

class ScreenOptions
    {
    public:
        ScreenOptions(OovStringRef const buildConfig, ProjectReader &project,
            GuiOptions &mGuiOptions);
        void optionsToScreen() const;
        void screenToOptions();

    private:
        ProjectReader &mProjectOptions;
        GuiOptions &mGuiOptions;
        std::vector<std::unique_ptr<Option>> mProjectOptionLookup;
        std::vector<std::unique_ptr<Option>> mGuiOptionLookup;
    };

ScreenOptions::ScreenOptions(OovStringRef const buildConfig, ProjectReader &project,
    GuiOptions &guiOptions):
    mProjectOptions(project),
    mGuiOptions(guiOptions)
    {
    BuildVariable buildVar;

    // First set base/global/default variables.

    buildVar.setVarName(OptCppCompilerPath);
    mProjectOptionLookup.push_back(std::unique_ptr<Option>(new EntryOption(
        buildVar.getVarFilterName(), "CompilerPathEntry")));

    buildVar.setVarName(OptJavaClassPath);
    mProjectOptionLookup.push_back(std::unique_ptr<Option>(new TextViewBuildOption(
        buildVar.getVarFilterName().getStr(), "JavaClassPathTextview")));

    buildVar.setVarName(OptJavaJdkPath);
    mProjectOptionLookup.push_back(std::unique_ptr<Option>(new TextViewBuildOption(
        buildVar.getVarFilterName().getStr(), "JavaJdkPathTextview")));

    buildVar.setVarName(OptCppLibPath);
    mProjectOptionLookup.push_back(std::unique_ptr<Option>(new EntryOption(
        buildVar.getVarFilterName().getStr(), "LibraryPathEntry")));

    buildVar.setVarName(OptObjSymbolPath);
    mProjectOptionLookup.push_back(std::unique_ptr<Option>(new EntryOption(
        buildVar.getVarFilterName().getStr(), "SymbolPathEntry")));

    buildVar.setVarName(OptCppArgs);
    mProjectOptionLookup.push_back(std::unique_ptr<Option>(new TextViewBuildOption(
        buildVar.getVarFilterName().getStr(), "CppArgumentsTextview")));


    // Now set filtered variables.

    buildVar.clearFilters();
    buildVar.setVarName(OptCppArgs);
    buildVar.setFunction(BuildVariable::F_Append);
    buildVar.addFilter(OptFilterNameBuildConfig, buildConfig);
    mProjectOptionLookup.push_back(std::unique_ptr<Option>(new TextViewBuildOption(
        buildVar.getVarFilterName().getStr(), "ExtraBuildArgsTextview")));

    buildVar.clearFilters();
    buildVar.setVarName(OptJavaArgs);
    buildVar.addFilter(OptFilterNameBuildMode, OptFilterValueBuildModeAnalyze);
    mProjectOptionLookup.push_back(std::unique_ptr<Option>(new TextViewBuildOption(
        buildVar.getVarFilterName().getStr(), "JavaExtraAnalysisArgsTextview")));

    buildVar.clearFilters();
    buildVar.setVarName(OptJavaArgs);
    buildVar.addFilter(OptFilterNameBuildMode, OptFilterValueBuildModeBuild);
    mProjectOptionLookup.push_back(std::unique_ptr<Option>(new TextViewBuildOption(
        buildVar.getVarFilterName().getStr(), "JavaExtraBuildArgsTextview")));

    // Editor
    mGuiOptionLookup.push_back(std::unique_ptr<Option>(new EntryOption(
        OptGuiEditorPath, "EditorPathEntry")));
    mGuiOptionLookup.push_back(std::unique_ptr<Option>(new EntryOption(
        OptGuiEditorLineArg, "EditorLineArgEntry")));
    mProjectOptionLookup.push_back(std::unique_ptr<Option>(new EntryOption(
        OptExeDebuggerPath, "DebuggerPathEntry")));

    mGuiOptionLookup.push_back(std::unique_ptr<Option>(new CheckOption(
        OptGuiShowAttributes, "ShowAttributesCheckbutton")));
    mGuiOptionLookup.push_back(std::unique_ptr<Option>(new CheckOption(
        OptGuiShowOperations, "ShowOperationsCheckbutton")));
    mGuiOptionLookup.push_back(std::unique_ptr<Option>(new CheckOption(
        OptGuiShowOperParams, "ShowOperParamsCheckbutton")));
    mGuiOptionLookup.push_back(std::unique_ptr<Option>(new CheckOption(
        OptGuiShowOperReturn, "ShowOperReturnCheckbutton")));
    mGuiOptionLookup.push_back(std::unique_ptr<Option>(new CheckOption(
        OptGuiShowAttrTypes, "ShowAttrTypesCheckbutton")));
    mGuiOptionLookup.push_back(std::unique_ptr<Option>(new CheckOption(
        OptGuiShowOperTypes, "ShowOperTypesCheckbutton")));
    mGuiOptionLookup.push_back(std::unique_ptr<Option>(new CheckOption(
        OptGuiShowPackageName, "ShowPackageNameCheckbutton")));

    mGuiOptionLookup.push_back(std::unique_ptr<Option>(new CheckOption(
        OptGuiShowOovSymbols, "ShowOovSymbolsCheckbutton")));
    mGuiOptionLookup.push_back(std::unique_ptr<Option>(new CheckOption(
        OptGuiShowTemplateRelations, "ShowTemplateRelationsCheckbutton")));
    mGuiOptionLookup.push_back(std::unique_ptr<Option>(new CheckOption(
        OptGuiShowOperParamRelations, "ShowOperParamRelationsCheckbutton")));
    mGuiOptionLookup.push_back(std::unique_ptr<Option>(new CheckOption(
        OptGuiShowOperBodyVarRelations, "ShowOperBodyVarRelationsCheckbutton")));
    mGuiOptionLookup.push_back(std::unique_ptr<Option>(new CheckOption(
        OptGuiShowRelationKey, "ShowRelationKeyCheckbutton")));
    }

void ScreenOptions::optionsToScreen() const
    {
    for(auto const &opt : mProjectOptionLookup)
        opt->optionToScreen(mProjectOptions);
    for(auto const &opt : mGuiOptionLookup)
        opt->optionToScreen(mGuiOptions);
    }

void ScreenOptions::screenToOptions()
    {
    for(auto const &opt : mProjectOptionLookup)
        opt->screenToOption(mProjectOptions);
    for(auto const &opt : mGuiOptionLookup)
        opt->screenToOption(mGuiOptions);
    }

static void enableBuildWidgets(bool enable)
    {
    gtk_widget_set_sensitive(Builder::getBuilder()->getWidget("CompilerPathEntry"),
            enable);
    gtk_widget_set_sensitive(Builder::getBuilder()->getWidget("LibraryPathEntry"),
            enable);
    gtk_widget_set_sensitive(Builder::getBuilder()->getWidget("SymbolPathEntry"),
            enable);
    }

///////////////////

OptionsDialog::OptionsDialog(ProjectReader &project, GuiOptions &guiOptions):
    mProjectOptions(project), mGuiOptions(guiOptions), mDialogRunning(false)
    {
    gOptionsDlg = this;
    mBuildConfigList.init(*Builder::getBuilder(), "BuildConfigTreeview", "Build Configurations");
    updateBuildConfig();
    }

OptionsDialog::~OptionsDialog()
    {}

void OptionsDialog::setBuildConfig()
    {
    if(mDialogRunning && Builder::getBuilder())
        {
            {
            ScreenOptions options(mCurrentBuildConfig, mProjectOptions,
                mGuiOptions);
            options.screenToOptions();
            }
        mCurrentBuildConfig = mBuildConfigList.getSelected();
            {
            ScreenOptions options(mCurrentBuildConfig, mProjectOptions,
                mGuiOptions);
            options.optionsToScreen();
            }
        enableBuildWidgets(mCurrentBuildConfig != BuildConfigAnalysis);
        gtk_widget_queue_draw(Builder::getBuilder()->getWidget("BuildArgumentsViewport"));
        }
    }

extern "C" G_MODULE_EXPORT void menuItemCallback(GtkWidget *button, gpointer data)
    {
    if(gOptionsDlg)
        {
        char const * const label = gtk_menu_item_get_label(GTK_MENU_ITEM(button));
        char const * p = strchr(label, ' ');
        if(p)
            p++;
        gOptionsDlg->buildConfig(reinterpret_cast<char const * const>(p));
        }
    }

void OptionsDialog::updateBuildMenu(OovStringVec &names)
    {
    for(auto const &item : mMenuItems)
        {
        gtk_widget_destroy(item);
        }
    mMenuItems.clear();
    GtkMenu *buildMenu = GTK_MENU(Builder::getBuilder()->getWidget("BuildMenu"));
    for(auto const &name : names)
        {
        OovString menuName = std::string("Build ") + name;
        GtkWidget *newItem = gtk_menu_item_new_with_label(menuName.c_str());
        mMenuItems.push_back(newItem);
        gtk_menu_shell_append(GTK_MENU_SHELL(buildMenu), newItem);
        g_signal_connect(newItem, "activate", G_CALLBACK(menuItemCallback), nullptr);
        gtk_widget_show(newItem);
        }
    }

void OptionsDialog::updateBuildConfig()
    {
    if(mBuildConfigList.getSelected().length() == 0)
        {
        mCurrentBuildConfig = BuildConfigAnalysis;
        mBuildConfigList.setSelected(BuildConfigAnalysis);
        }

    mBuildConfigList.clear();
    mBuildConfigList.appendText(BuildConfigAnalysis);
    mBuildConfigList.appendText(BuildConfigDebug);
    mBuildConfigList.appendText(BuildConfigRelease);

    std::string confStr = mProjectOptions.getValue(OptBuildConfigs);
    CompoundValue configVals;
    configVals.parseString(confStr);
    for(const auto &config:configVals)
        {
        mBuildConfigList.appendText(config);
        }
    updateBuildMenu(configVals);
    }

void OptionsDialog::showScreen()
    {
    Dialog dlg(GTK_DIALOG(Builder::getBuilder()->getWidget("OptionsDialog")));
    BuildVariablesDialog buildVarDlg(mProjectOptions, GTK_WINDOW(dlg.getDialog()));

    updateBuildConfig();
    moveOptionsToScreen();
    enableBuildWidgets(mCurrentBuildConfig != BuildConfigAnalysis);
    mDialogRunning = true;
    dlg.run();
    mDialogRunning = false;
    }

void OptionsDialog::moveOptionsToScreen()
    {
    ScreenOptions options(mCurrentBuildConfig, mProjectOptions, mGuiOptions);
    mBuildConfigList.setSelected(mCurrentBuildConfig);
    options.optionsToScreen();
    }

void OptionsDialog::newConfig()
    {
    Dialog dlg(GTK_DIALOG(Builder::getBuilder()->getWidget("NewBuildConfigDialog")));
    GtkEntry *oldNameEntry = GTK_ENTRY(Builder::getBuilder()->getWidget("OldConfigNameEntry"));
    Gui::setText(oldNameEntry, mBuildConfigList.getSelected());
    if(dlg.run())
        {
        }
    }

void OptionsDialog::addConfig()
    {
    GtkEntry *newNameEntry = GTK_ENTRY(Builder::getBuilder()->getWidget("NewConfigNameEntry"));

    // Update the build config option
    std::string compStr = mProjectOptions.getValue(OptBuildConfigs);
    CompoundValue compVal;
    compVal.parseString(compStr);
    OovString newName = Gui::getText(newNameEntry);

    OovStringVec cfgs = compVal;
    cfgs.push_back(BuildConfigAnalysis);
    cfgs.push_back(BuildConfigDebug);
    cfgs.push_back(BuildConfigRelease);
    bool found = std::find(cfgs.begin(), cfgs.end(), newName) != cfgs.end();
    if(!found)
        {
        compVal.addArg(newName);
        mProjectOptions.setNameValue(OptBuildConfigs, compVal.getAsString());

        // Leave what is on the screen, and change the config name.Save the
        // screen data to the new config.
        mCurrentBuildConfig = newName;
    //    ScreenOptions options(mCurrentBuildConfig);
     //   options.screenToOptions();

        updateBuildConfig();
        }
    else
        Gui::messageBox("Configuration already exists", GTK_MESSAGE_INFO);
    }

void OptionsDialog::saveScreen()
    {
    ScreenOptions options(mCurrentBuildConfig, mProjectOptions, mGuiOptions);
    options.screenToOptions();

    OovStatus status = mProjectOptions.writeFile();
    if(!status.ok())
        {
        OovString errStr = "Unable to write project options file: ";
        errStr += mProjectOptions.getFilename();
        OovError::report(ET_Error, errStr);
        }
    if(status.ok())
        {
        status = mGuiOptions.writeFile();
        if(!status.ok())
            {
            OovString errStr = "Unable to write GUI options file: ";
            errStr += mGuiOptions.getFilename();
            OovError::report(ET_Error, errStr);
            }
        }
    updateOptions();
    }

void OptionsDialog::runPackagesDialog()
    {
    GtkTextView *view = GTK_TEXT_VIEW(Builder::getBuilder()->getWidget("CppArgumentsTextview"));
    std::string str = Gui::getText(view);
    ProjectPackagesDialog dlg(str);
    if(dlg.run(true))
        {
        Gui::setText(view, str);
        }
    }


/////////////////


extern "C" G_MODULE_EXPORT void on_AnalysisAdvancedButton_clicked(
    GtkWidget *button, gpointer data)
    {
    if(BuildVariablesDialog::runAdvancedDialog())
        {
        if(gOptionsDlg)
            gOptionsDlg->moveOptionsToScreen();
        }
    }

extern "C" G_MODULE_EXPORT void on_EditOptionsmenuitem_activate()
    {
    if(gOptionsDlg)
        gOptionsDlg->showScreen();
    }

extern "C" G_MODULE_EXPORT void on_OptionsOkButton_clicked(
    GtkWidget * /*button*/, gpointer /*data*/)
    {
    if(gOptionsDlg)
        gOptionsDlg->saveScreen();
    gtk_widget_hide(Builder::getBuilder()->getWidget("OptionsDialog"));
    }

extern "C" G_MODULE_EXPORT void on_OptionsCancelButton_clicked(
    GtkWidget * /*button*/, gpointer /*data*/)
    {
    gtk_widget_hide(Builder::getBuilder()->getWidget("OptionsDialog"));
    }

extern "C" G_MODULE_EXPORT void on_BuildConfigTreeview_cursor_changed(
    GtkWidget * /*button*/, gpointer /*data*/)
    {
    if(gOptionsDlg)
        gOptionsDlg->setBuildConfig();
    }

extern "C" G_MODULE_EXPORT void on_ExternalPackagesButton_clicked(
    GtkWidget * /*button*/, gpointer /*data*/)
    {
    if(gOptionsDlg)
        gOptionsDlg->runPackagesDialog();
    }

/////////////// New config //////////////

extern "C" G_MODULE_EXPORT void on_BuildConfigNewButton_clicked(
    GtkWidget * /*button*/, gpointer /*data*/)
    {
    if(gOptionsDlg)
        {
        gOptionsDlg->newConfig();
        }
    }

extern "C" G_MODULE_EXPORT void on_NewBuildConfigOKButton_clicked(
    GtkWidget * /*button*/, gpointer /*data*/)
    {
    if(gOptionsDlg)
        {
        gOptionsDlg->addConfig();
        }
    gtk_widget_hide(Builder::getBuilder()->getWidget("NewBuildConfigDialog"));
    }

extern "C" G_MODULE_EXPORT void on_NewBuildConfigCancelButton_clicked(
    GtkWidget * /*button*/, gpointer /*data*/)
    {
    gtk_widget_hide(Builder::getBuilder()->getWidget("NewBuildConfigDialog"));
    }


////////////////////////

struct PrefOption
    {
    int offset;
    char const * const widgetName;
    };

static PrefOption PrefOptions[] =
    {
    { offsetof(ClassNodeDrawOptions, drawAttributes), "ShowAttributesCheckbutton"},
    { offsetof(ClassNodeDrawOptions, drawOperations), "ShowOperationsCheckbutton" },
    { offsetof(ClassNodeDrawOptions, drawOperParams), "ShowOperParamsCheckbutton" },
    { offsetof(ClassNodeDrawOptions, drawOperReturn), "ShowOperReturnCheckbutton" },
    { offsetof(ClassNodeDrawOptions, drawAttrTypes), "ShowAttrTypesCheckbutton" },
    { offsetof(ClassNodeDrawOptions, drawOperTypes), "ShowOperTypesCheckbutton" },
    { offsetof(ClassNodeDrawOptions, drawPackageName), "ShowPackageNameCheckbutton" },
    };

bool ClassPreferencesDialog::run(Builder &builder, ClassNodeDrawOptions &options)
    {
    Dialog dlg(GTK_DIALOG(gtk_dialog_new()));
    dlg.addButton(GUI_CANCEL, GTK_RESPONSE_CANCEL);
    dlg.addButton(GUI_OK, GTK_RESPONSE_OK);

    // Steal the options, then put them back.
    GtkWidget *optionsBox = builder.getWidget("ClassesOptionsBox");
    Gui::reparentWidget(optionsBox, GTK_CONTAINER(dlg.getContentArea()));

    for(auto const &opt : PrefOptions)
        {
        bool active = *(reinterpret_cast<unsigned char *>(&options) + (opt.offset));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
                Builder::getBuilder()->getWidget(opt.widgetName)), active);
        }

    bool ok = dlg.run();
    if(ok)
        {
        for(auto const &opt : PrefOptions)
            {
            bool active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
                    Builder::getBuilder()->getWidget(opt.widgetName)));
            *(reinterpret_cast<unsigned char *>(&options) + (opt.offset)) = active;
            }
        }
    Gui::reparentWidget(optionsBox, GTK_CONTAINER(builder.getWidget("OptionsNotebook")));
    dlg.destroy();
    return ok;
    }

