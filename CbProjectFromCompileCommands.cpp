/*
 * This file uses parts of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#include <sdk.h>  // Code::Blocks SDK

#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/utils.h>
#include <wx/filename.h>
#include <wx/fs_zip.h>
#include <wx/menu.h>
#include <wx/xrc/xmlres.h>
#include <wx/dir.h>

#include <configurationpanel.h>
#include "CbProjectFromCompileCommands.h"
#include "projectmanager.h"
#include "logmanager.h"
#include "cbproject.h"
#include "globals.h"
#include "filefilters.h"
#include "filegroupsandmasks.h"
#include "multiselectdlg.h"
#include <fstream>
#include <sstream>
#include "nlohmann/json.hpp"
#define DEBUG

using json = nlohmann::json;

// Register the plugin with Code::Blocks.
// We are using an anonymous namespace so we don't litter the global one.
namespace
{
PluginRegistrant<CbProjectFromCompileCommands> reg(_T("CbProjectFromCompileCommands"));
const int idCbProjectFromCompileCommands = wxNewId();
}  // namespace

BEGIN_EVENT_TABLE(CbProjectFromCompileCommands, cbPlugin)
EVT_MENU(idCbProjectFromCompileCommands, CbProjectFromCompileCommands::OnCbProjectFromCompileCommands)
END_EVENT_TABLE()

const char* pluginName = "CbProjectFromCompileCommands";

CbProjectFromCompileCommands::CbProjectFromCompileCommands()
{
    // Make sure our resources are available.
    // In the generated boilerplate code we have no resources but when
    // we add some, it will be nice that this code is in place already ;)
    if (!Manager::LoadResource(_T("CbProjectFromCompileCommands.zip")))
    {
        NotifyMissingFile(_T("CbProjectFromCompileCommands.zip"));
    }
}

void CbProjectFromCompileCommands::BuildMenu(wxMenuBar* menuBar)
{
    LogManager* logManager = Manager::Get()->GetLogManager();

    if (!IsAttached() || !menuBar)
    {
        logManager->LogError(wxString::Format(_("%s Preconditions not met!"), pluginName));
        return;
    }

    const int fileMenuIndex = menuBar->FindMenu(_("&File"));
    if (fileMenuIndex == wxNOT_FOUND)
    {
        logManager->LogError(wxString::Format(_("%s Could not get File menu Idx!"), pluginName));
        return;
    }

    wxMenu* fileMenu = menuBar->GetMenu(fileMenuIndex);
    if (!fileMenu)
    {
        logManager->LogError(wxString::Format(_("%s Could not get File menu!"), pluginName));
        return;
    }

    const int pluginMenuId = fileMenu->FindItem(_("&Create Project from compile commands"));
    if (pluginMenuId == wxNOT_FOUND)
    {
        wxMenuItemList menuItems = fileMenu->GetMenuItems();
        const int menuId = fileMenu->FindItem(_("R&ecent files"));
        wxMenuItem* recentFileItem = fileMenu->FindItem(menuId);
        int id = menuItems.IndexOf(recentFileItem);
        if (id == wxNOT_FOUND)
        {
            id = 7;
        }
        else
        {
            ++id;
        }
        // The position is hard-coded to "Recent Files" menu. Please adjust it if necessary
        fileMenu->Insert(++id, idCbProjectFromCompileCommands, _("&Create Project from compile commands"),
                         _("&Create Project from compile commands"));
        fileMenu->InsertSeparator(++id);
    }
    else
    {
        logManager->LogError(wxString::Format(_("%s Menu already present!"), pluginName));
    }
}

void CbProjectFromCompileCommands::OnCbProjectFromCompileCommands(wxCommandEvent& event)
{
    wxString errorString;
    if (!CreateCbProjectFromCompileCommands(errorString))
    {
        if (!errorString.IsEmpty())
        {
            Manager::Get()->GetLogManager()->LogError(errorString);
            cbMessageBox(_(errorString), _("Error"), wxICON_ERROR);
        }
    }
}

bool CbProjectFromCompileCommands::CreateProjectInternal(const wxString& fileName, const wxArrayString& filelist, wxString& errorString)
{
    bool ret;
    ProjectManager* projectManager = Manager::Get()->GetProjectManager();
    cbProject* prj = projectManager->NewProject(fileName);
    if (!prj)
    {
        errorString = wxString::Format(_("%s : Could not create project!"), pluginName);
        ret = false;
    }
    else
    {
        prj->SetMakefileCustom(true);
        prj->AddBuildTarget("all");

        wxArrayInt targets;
        targets.Add(0);
#ifdef DEBUG
        for (const wxString& file : filelist)
        {
            fprintf(stderr, "%s\n", file.ToUTF8().data());
        }
#endif  // DEBUG

        projectManager->AddMultipleFilesToProject(filelist, prj, targets);
        Manager::Get()->GetLogManager()->Log(wxString::Format(_("%s Added %d files!"), pluginName, (int)filelist.GetCount()));
        prj->SetModified(true);
        prj->Save();

        if (!projectManager->IsLoadingWorkspace())
        {
            projectManager->SetProject(prj);
            /*Changes from Pecan - instead of PM SetProject, close and open projects
            https://forums.codeblocks.org/index.php/topic,25509.msg173704.html#msg173704
            */
            bool ok = projectManager->CloseProject(prj, true, true);
            if (!ok)
            {
                errorString = wxString::Format(_("%s : Could not close project!"), pluginName);
                ret = false;
            }
            cbProject* prj = projectManager->LoadProject(fileName, true);
            if (!prj)
            {
                errorString = wxString::Format(_("%s : Could not open project!"), pluginName);
                ret = false;
            }
        }
        projectManager->GetUI().RebuildTree();
        ret = true;
    }
    return ret;
}

bool CbProjectFromCompileCommands::CreateCbProjectFromCompileCommands(wxString& errorString)
{
    LogManager* logManager = Manager::Get()->GetLogManager();
    logManager->Log(wxString::Format(_("%s Yay!"), pluginName));

    wxFileDialog openFileDialog(nullptr, _("Open compile_commands.json file"), wxEmptyString, wxEmptyString,
                                "JSON files (*.json)|*.json|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (openFileDialog.ShowModal() == wxID_CANCEL)
    {
        errorString = wxString::Format(_("%s: User canceled !"), pluginName);
        return false;
    }
    wxFileDialog saveFileDialog(nullptr, _("Save Project file"), openFileDialog.GetDirectory(), "", "CBP files (*.cbp)|*.cbp",
                                wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (saveFileDialog.ShowModal() == wxID_CANCEL)
    {
        errorString = wxString::Format(_("%s: User canceled !"), pluginName);
        return false;
    }

    std::ifstream f(openFileDialog.GetPath());
    json jArray = json::parse(f);
    f.close();

    wxFileName cbpNameFull = saveFileDialog.GetPath();
    wxString projectDirectory = cbpNameFull.GetPath();

    bool generateCompileCommands = (projectDirectory != openFileDialog.GetDirectory());

    wxArrayString array;
    for (size_t i = 0; i < jArray.size(); i++)
    {
        json& jentry = jArray.at(i);
        std::string jDirectory = jentry.at("directory").get<std::string>();
        wxFileName fileName(jentry.at("file").get<std::string>());

#ifdef DEBUG
        fprintf(stderr, "file idx %zu name %s\n", i, fileName.GetFullPath().ToUTF8().data());
#endif
        if (fileName.IsRelative())
        {
            fileName.Assign(jDirectory + wxFILE_SEP_PATH + fileName.GetFullName());
#ifdef DEBUG
            fprintf(stderr, "file idx %zu appended directory . name %s\n", i, fileName.GetFullPath().ToUTF8().data());
#endif
        }
        fileName.MakeRelativeTo(projectDirectory);
#ifdef DEBUG
        fprintf(stderr, "file idx %zu after make relative . name %s\n", i, fileName.GetFullPath().ToUTF8().data());
#endif
        array.push_back(fileName.GetFullPath());

        if (generateCompileCommands)
        {
            jentry["file"] = fileName.GetFullPath().ToStdString();
            jentry["directory"] = projectDirectory.ToStdString();

            if (jentry.contains("command"))
            {
                std::ostringstream updatedCommand;
                std::istringstream iss(jentry.at("command").get<std::string>());
                std::string entry;

                while (getline(iss, entry, ' '))
                {
#ifdef DEBUG
                    fprintf(stderr, "file idx %zu entry %s\n", i, entry.c_str());
#endif
                    if (entry.rfind("-I", 0) == 0)
                    {
#ifdef DEBUG
                        fprintf(stderr, "file idx %zu matched. entry %s\n", i, entry.c_str());
#endif
                        std::string includePath = entry.substr(2);
                        includePath.erase(includePath.begin(),
                                          std::find_if(includePath.begin(), includePath.end(), [](unsigned char ch) { return !std::isspace(ch); }));
                        wxFileName includeDirectory(includePath);
#ifdef DEBUG
                        fprintf(stderr, "file idx %zu includePath %s\n", i, includeDirectory.GetFullPath().ToUTF8().data());
#endif
                        if (includeDirectory.IsRelative())
                        {
                            includeDirectory.Assign(jDirectory + wxFILE_SEP_PATH + includeDirectory.GetFullName());
#ifdef DEBUG
                            fprintf(stderr, "file idx %zu appended directory . includeDirectory %s\n", i,
                                    includeDirectory.GetFullPath().ToUTF8().data());
#endif
                        }
                        includeDirectory.MakeRelativeTo(projectDirectory);
#ifdef DEBUG
                        fprintf(stderr, "file idx %zu after make relative . includeDirectory %s\n", i,
                                includeDirectory.GetFullPath().ToUTF8().data());
#endif
                        updatedCommand << "-I" << includeDirectory.GetFullPath().ToStdString();
                    }
                    else
                    {
                        updatedCommand << entry;
                    }
                    updatedCommand << ' ';
                }
                jentry["command"] = updatedCommand.str();
#ifdef DEBUG
                fprintf(stderr, "file idx %zu updated command %s\n", i, updatedCommand.str().c_str());
#endif
            }
        }
    }

    cbpNameFull.SetExt(FileFilters::CODEBLOCKS_EXT);
    logManager->Log(wxString::Format(_("%s cbpNameFull %s!"), pluginName, cbpNameFull.GetFullPath()));

    bool projectSetupCompleted = false;
    // generate list of files to add
    if (array.IsEmpty())
    {
        errorString = wxString::Format(_("%s : Could not find any files!"), pluginName);
    }
    else
    {
        logManager->Log(wxString::Format(_("%s : Before filter %d files present!"), pluginName, (int)array.GetCount()));

        wxString wild;
        const FilesGroupsAndMasks* fgam = Manager::Get()->GetProjectManager()->GetFilesGroupsAndMasks();
        for (unsigned fm_idx = 0; fm_idx < fgam->GetGroupsCount(); fm_idx++) wild += fgam->GetFileMasks(fm_idx);

        MultiSelectDlg dlg(nullptr, array, wild, _("Select the files to add to the project:"));
        PlaceWindow(&dlg);
        if (dlg.ShowModal() != wxID_OK)
        {
            logManager->Log(wxString::Format(_("%s : Dialog ShowModal canceled!"), pluginName));
        }
        else
        {
            array = dlg.GetSelectedStrings();

            if (array.IsEmpty())
            {
                errorString = wxString::Format(_("%s : No files selected!"), pluginName);
            }
            else
            {
                if (generateCompileCommands)
                {
                    wxString compileCommandsFullPath = projectDirectory + wxFILE_SEP_PATH + "compile_commands.json";
                    std::ofstream jsonFile(compileCommandsFullPath.ToStdString());
                    if (!jsonFile.is_open())
                    {
                        wxString msg(wxString::Format(_("'compile_commands.json' file %s\nfailed to open for output.\n"), compileCommandsFullPath));
                        cbMessageBox(msg);
                    }
                    else
                    {
                        jsonFile << std::setw(4) << jArray;
                    }
                }
                projectSetupCompleted = CreateProjectInternal(cbpNameFull.GetFullPath(), array, errorString);
            }
        }
    }
    return projectSetupCompleted;
}

void CbProjectFromCompileCommands::OnAttach() {}

void CbProjectFromCompileCommands::OnRelease(bool appShutDown) {}
