/*
 * This file uses parts of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef CbProjectFromCompileCommands_H_INCLUDED
#define CbProjectFromCompileCommands_H_INCLUDED

// For compilers that support precompilation, includes <wx/wx.h>
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include "cbplugin.h"

class CbProjectFromCompileCommands : public cbPlugin
{
   public:
    /** Constructor. */
    CbProjectFromCompileCommands();
    ~CbProjectFromCompileCommands() = default;

    void BuildMenu(wxMenuBar* menuBar) override;

   protected:
    void OnAttach() override;

    void OnRelease(bool appShutDown) override;
    DECLARE_EVENT_TABLE()

   private:
    void OnCbProjectFromCompileCommands(wxCommandEvent& event);
    bool CreateCbProjectFromCompileCommands(wxString& error);
    bool CreateProjectInternal(const wxString& fileName, const wxArrayString& filelist, wxString& errorString);
};

#endif  // CbProjectFromCompileCommands_H_INCLUDED
