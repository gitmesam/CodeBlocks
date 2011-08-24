/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision$
 * $Id$
 * $HeadURL$
 */

#include "sdk.h"
#ifndef CB_PRECOMP
    #include <wx/app.h>
    #include <wx/dnd.h>
    #include <wx/fontutil.h>
    #include <wx/menu.h>
    #include <wx/sizer.h>

    #include "cbexception.h"
    #include "cbplugin.h"
    #include "logmanager.h"
    #include "scrollingdialog.h"
#endif

#include <numeric>
#include <map>

#include <wx/propgrid/propgrid.h>

#include "watchesdlg.h"

#include "debuggermanager.h"

namespace
{
    const long idGrid = wxNewId();
    const long idTooltipGrid = wxNewId();
    const long idTooltipTimer = wxNewId();

    const long idMenuRename = wxNewId();
    const long idMenuProperties = wxNewId();
    const long idMenuDelete = wxNewId();
    const long idMenuDeleteAll = wxNewId();
}

BEGIN_EVENT_TABLE(WatchesDlg, wxPanel)
    EVT_PG_ITEM_EXPANDED(idGrid, WatchesDlg::OnExpand)
    EVT_PG_ITEM_COLLAPSED(idGrid, WatchesDlg::OnCollapse)
    EVT_PG_SELECTED(idGrid, WatchesDlg::OnPropertySelected)
    EVT_PG_CHANGED(idGrid, WatchesDlg::OnPropertyChanged)
    EVT_PG_CHANGING(idGrid, WatchesDlg::OnPropertyChanging)
    EVT_PG_LABEL_EDIT_BEGIN(idGrid, WatchesDlg::OnPropertyLableEditBegin)
    EVT_PG_LABEL_EDIT_ENDING(idGrid, WatchesDlg::OnPropertyLableEditEnd)
    EVT_PG_RIGHT_CLICK(idGrid, WatchesDlg::OnPropertyRightClick)
    EVT_IDLE(WatchesDlg::OnIdle)

    EVT_MENU(idMenuRename, WatchesDlg::OnMenuRename)
    EVT_MENU(idMenuProperties, WatchesDlg::OnMenuProperties)
    EVT_MENU(idMenuDelete, WatchesDlg::OnMenuDelete)
    EVT_MENU(idMenuDeleteAll, WatchesDlg::OnMenuDeleteAll)
END_EVENT_TABLE()


class cbTextCtrlAndButtonTooltipEditor : public wxPGTextCtrlAndButtonEditor
{
    DECLARE_DYNAMIC_CLASS(wxTextCtrlAndButtonTooltipEditor)
public:
    virtual wxPG_CONST_WXCHAR_PTR GetName() const;

    virtual wxPGWindowList CreateControls(wxPropertyGrid* propgrid, wxPGProperty* property,
                                          const wxPoint& pos, const wxSize& sz) const
    {
        wxPGWindowList const &list = wxPGTextCtrlAndButtonEditor::CreateControls(propgrid, property, pos, sz);

        list.m_secondary->SetToolTip(_("Click the button to see the value.\n"
                                       "Hold CONTROL to see the raw output string returned by the debugger.\n"
                                       "Hold SHIFT to see debugging representation of the cbWatch object."));
        return list;
    }

};

WX_PG_DECLARE_EDITOR(cbTextCtrlAndButtonTooltip);
WX_PG_IMPLEMENT_EDITOR_CLASS(cbTextCtrlAndButtonTooltip, cbTextCtrlAndButtonTooltipEditor, wxPGTextCtrlAndButtonEditor);

class WatchesProperty : public wxStringProperty
{
    public:
        WatchesProperty(const wxString& label, const wxString& value, cbWatch *watch, bool readonly) :
            wxStringProperty(label, wxPG_LABEL, value),
            m_watch(watch),
            m_readonly(readonly)
        {
        }

        // Set editor to have button
        virtual const wxPGEditor* DoGetEditorClass() const
        {
            return m_readonly ? nullptr : wxPG_EDITOR(cbTextCtrlAndButtonTooltip);
        }

        // Set what happens on button click
        virtual wxPGEditorDialogAdapter* GetEditorDialog() const;

        cbWatch* GetWatch() { return m_watch; }
        const cbWatch* GetWatch() const { return m_watch; }
        void SetWatch(cbWatch* watch) { m_watch = watch; }

    protected:
        cbWatch *m_watch;
        bool m_readonly;
};

class WatchRawDialogAdapter : public wxPGEditorDialogAdapter
{
    public:

        WatchRawDialogAdapter()
        {
        }

        virtual bool DoShowDialog(wxPropertyGrid* WXUNUSED(propGrid), wxPGProperty* property);

    protected:
};

/// @breif dialog to show the value of a watch
class WatchRawDialog : public wxScrollingDialog
{
    private:
        enum Type
        {
            TypeNormal,
            TypeDebug,
            TypeWatchTree
        };
    public:
        static WatchRawDialog* Create(const WatchesProperty* watch)
        {
            cbAssert(watch->GetWatch());

            WatchRawDialog *dlg;
            Map::iterator it = s_dialogs.find(watch->GetWatch());
            if (it != s_dialogs.end())
                dlg = it->second;
            else
            {
                dlg = new WatchRawDialog;
                s_dialogs[watch->GetWatch()] = dlg;
            }

            dlg->m_type = TypeNormal;

            if (wxGetKeyState(WXK_CONTROL))
                dlg->m_type = TypeDebug;
            else if (wxGetKeyState(WXK_SHIFT))
                dlg->m_type = TypeWatchTree;

            dlg->SetTitle(wxString::Format(wxT("Watch '%s' raw value"), watch->GetName().c_str()));
            dlg->SetValue(watch);
            dlg->Raise();

            return dlg;
        }

        static void UpdateValue(const WatchesProperty* watch)
        {
            Map::iterator it = s_dialogs.find(watch->GetWatch());
            if (it != s_dialogs.end())
                it->second->SetValue(watch);
        }
    private:
        WatchRawDialog() :
            wxScrollingDialog(Manager::Get()->GetAppWindow(),
                              wxID_ANY,
                              wxEmptyString,
                              wxDefaultPosition,
                              wxSize(400, 400),
                              wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
            m_type(TypeNormal)
        {
            wxBoxSizer *bs = new wxBoxSizer(wxVERTICAL);
            m_text = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                    wxTE_MULTILINE | wxTE_READONLY);
            bs->Add(m_text, 1, wxEXPAND | wxALL, 5);
            SetAutoLayout(TRUE);
            SetSizer(bs);
        }

        void OnClose(wxCloseEvent &event)
        {
            for (Map::iterator it = s_dialogs.begin(); it != s_dialogs.end(); ++it)
            {
                if (it->second == this)
                {
                    s_dialogs.erase(it);
                    break;
                }
            }
            Destroy();
        }

        void SetValue(const WatchesProperty* watch)
        {
            switch (m_type)
            {
                case TypeNormal:
                    m_text->SetValue(watch->GetValueAsString(wxPG_FULL_VALUE));
                    break;

                case TypeDebug:
                    m_text->SetValue(watch->GetWatch()->GetDebugString());
                    break;

                case TypeWatchTree:
                    {
                        wxString value;
                        WatchToString(value, *watch->GetWatch());
                        m_text->SetValue(value);
                    }
                    break;
            }
        }

        static void WatchToString(wxString &result, const cbWatch &watch, const wxString &indent = wxEmptyString)
        {
            wxString sym, value;
            watch.GetSymbol(sym);
            watch.GetValue(value);

            result += indent + wxT("[symbol = ") + sym + wxT("]\n");
            result += indent + wxT("[value = ") + value + wxT("]\n");
            result += indent + wxString::Format(wxT("[children = %d]\n"), watch.GetChildCount());

            for(int child_index = 0; child_index < watch.GetChildCount(); ++child_index)
            {
                const cbWatch *child = watch.GetChild(child_index);

                result += indent + wxString::Format(wxT("[child %d]\n"), child_index);
                WatchToString(result, *child, indent + wxT("    "));
            }
        }
    private:
        DECLARE_EVENT_TABLE()
    private:
        typedef std::map<cbWatch const*, WatchRawDialog*> Map;

        static Map s_dialogs;

        wxTextCtrl *m_text;
        Type        m_type;
};

WatchRawDialog::Map WatchRawDialog::s_dialogs;

BEGIN_EVENT_TABLE(WatchRawDialog, wxScrollingDialog)
    EVT_CLOSE(WatchRawDialog::OnClose)
END_EVENT_TABLE()


bool WatchRawDialogAdapter::DoShowDialog(wxPropertyGrid* WXUNUSED(propGrid), wxPGProperty* property)
{
    WatchesProperty *watch = static_cast<WatchesProperty*>(property);
    if (watch->GetWatch())
    {
        WatchRawDialog *dlg = WatchRawDialog::Create(watch);
        dlg->Show();
    }
    return false;
}

wxPGEditorDialogAdapter* WatchesProperty::GetEditorDialog() const
{
    return new WatchRawDialogAdapter();
}

class WatchesDropTarget : public wxTextDropTarget
{
public:
    virtual bool OnDropText(wxCoord x, wxCoord y, const wxString& text)
    {
        cbDebuggerPlugin *activeDebugger = Manager::Get()->GetDebuggerManager()->GetActiveDebugger();
        cbWatch::Pointer watch = activeDebugger->AddWatch(text);
        if (watch.get())
            Manager::Get()->GetDebuggerManager()->GetWatchesDialog()->AddWatch(watch.get());
        // we return false here to veto the operation, otherwise the dragged text might get cut,
        // because we use wxDrag_DefaultMove in ScintillaWX::StartDrag (seems to happen only on windows)
        return false;
    }
private:
};

WatchesDlg::WatchesDlg() :
    wxPanel(Manager::Get()->GetAppWindow(), -1),
    m_append_empty_watch(false)
{
    wxBoxSizer *bs = new wxBoxSizer(wxVERTICAL);
    m_grid = new wxPropertyGrid(this, idGrid, wxDefaultPosition, wxDefaultSize,
                                wxPG_SPLITTER_AUTO_CENTER | wxTAB_TRAVERSAL /*| wxWANTS_CHARS*/);

    m_grid->SetExtraStyle(wxPG_EX_DISABLE_TLP_TRACKING | wxPG_EX_HELP_AS_TOOLTIPS);
    m_grid->SetDropTarget(new WatchesDropTarget);
    m_grid->SetColumnCount(3);
    bs->Add(m_grid, 1, wxEXPAND | wxALL);
    SetAutoLayout(TRUE);
    SetSizer(bs);

    wxPGRegisterEditorClass(cbTextCtrlAndButtonTooltip);

    m_grid->SetColumnProportion(0, 40);
    m_grid->SetColumnProportion(1, 40);
    m_grid->SetColumnProportion(2, 20);

    wxPGProperty *prop = m_grid->Append(new WatchesProperty(wxEmptyString, wxEmptyString, NULL, false));
    m_grid->SetPropertyAttribute(prop, wxT("Units"), wxEmptyString);

    m_grid->Connect(idGrid, wxEVT_KEY_DOWN, wxKeyEventHandler(WatchesDlg::OnKeyDown), NULL, this);
}

void AppendChildren(wxPropertyGrid &grid, wxPGProperty &property, cbWatch &watch, bool readonly)
{
    for(int ii = 0; ii < watch.GetChildCount(); ++ii)
    {
        cbWatch &child = *watch.GetChild(ii);

        wxString symbol, value, type;
        child.GetSymbol(symbol);
        child.GetValue(value);
        child.GetType(type);

        wxPGProperty *prop = new WatchesProperty(symbol, value, &child, readonly);
        prop->SetExpanded(child.IsExpanded());
        wxPGProperty *new_prop = grid.AppendIn(&property, prop);
        grid.SetPropertyAttribute(new_prop, wxT("Units"), type);
        if (value.empty())
            grid.SetPropertyHelpString(new_prop, wxEmptyString);
        else
            grid.SetPropertyHelpString(new_prop, symbol + wxT("=") + value);

        if(child.IsChanged())
        {
            grid.SetPropertyTextColour(prop, wxColor(255, 0, 0));
            WatchRawDialog::UpdateValue(static_cast<const WatchesProperty*>(prop));
        }
        else
            grid.SetPropertyColourToDefault(prop);

        AppendChildren(grid, *prop, child, readonly);
    }
}

void UpdateWatch(wxPropertyGrid *grid, wxPGProperty *property, cbWatch *watch, bool readonly)
{
    if (!property)
        return;

    wxString value, symbol, type;
    watch->GetSymbol(symbol);
    watch->GetValue(value);
    property->SetValue(value);
    property->SetExpanded(watch->IsExpanded());
    watch->GetType(type);
    if(watch->IsChanged())
        grid->SetPropertyTextColour(property, wxColor(255, 0, 0));
    else
        grid->SetPropertyColourToDefault(property);
    grid->SetPropertyAttribute(property, wxT("Units"), type);
    if (value.empty())
        grid->SetPropertyHelpString(property, wxEmptyString);
    else
        grid->SetPropertyHelpString(property, symbol + wxT("=") + value);

    property->DeleteChildren();

    if (property->GetName() != symbol)
    {
        grid->SetPropertyName(property, symbol);
        grid->SetPropertyLabel(property, symbol);
    }

    AppendChildren(*grid, *property, *watch, readonly);

    WatchRawDialog::UpdateValue(static_cast<const WatchesProperty*>(property));
}

void SetValue(WatchesProperty *prop)
{
    if (prop)
    {
        cbWatch *watch = prop->GetWatch();
        if (watch)
        {
            cbDebuggerPlugin *plugin = Manager::Get()->GetDebuggerManager()->GetDebuggerHavingWatch(watch);
            if (plugin)
                plugin->SetWatchValue(watch, prop->GetValue());
        }
    }
}

void WatchesDlg::UpdateWatches()
{
    for(WatchItems::iterator it = m_watches.begin(); it != m_watches.end(); ++it)
         UpdateWatch(m_grid, it->property, it->watch, false);
    m_grid->Refresh();
}

void WatchesDlg::AddWatch(cbWatch *watch)
{
    wxPGProperty *last_prop = m_grid->GetLastProperty();

    WatchItem item;
    wxString symbol, value;
    watch->GetSymbol(symbol);

    if(last_prop && last_prop->GetLabel() == wxEmptyString)
    {
        item.property = last_prop;

        // if we are editing the label the calls SetPropertyLabel and SetPropertyName don't work,
        // so we stop the edit operationś
        if(m_grid->GetLabelEditor())
            m_grid->EndLabelEdit(0);
        m_grid->SetPropertyLabel(item.property, symbol);
        m_grid->SetPropertyName(item.property, symbol);

        WatchesProperty *watches_prop = static_cast<WatchesProperty*>(last_prop);
        watches_prop->SetWatch(watch);
        m_grid->Append(new WatchesProperty(wxEmptyString, wxEmptyString, NULL, false));
    }
    else
    {
        item.property = m_grid->Append(new WatchesProperty(symbol, value, watch, false));
    }

    item.property->SetExpanded(watch->IsExpanded());
    item.watch = watch;
    m_watches.push_back(item);
    m_grid->Refresh();
}

void WatchesDlg::OnExpand(wxPropertyGridEvent &event)
{
    WatchesProperty *prop = static_cast<WatchesProperty*>(event.GetProperty());
    prop->GetWatch()->Expand(true);

    cbDebuggerPlugin *plugin = Manager::Get()->GetDebuggerManager()->GetActiveDebugger();
    cbAssert(plugin);
    plugin->ExpandWatch(prop->GetWatch());
}

void WatchesDlg::OnCollapse(wxPropertyGridEvent &event)
{
    WatchesProperty *prop = static_cast<WatchesProperty*>(event.GetProperty());
    prop->GetWatch()->Expand(false);

    cbDebuggerPlugin *plugin = Manager::Get()->GetDebuggerManager()->GetActiveDebugger();
    cbAssert(plugin);
    plugin->CollapseWatch(prop->GetWatch());
}

void WatchesDlg::OnPropertyChanged(wxPropertyGridEvent &event)
{
    WatchesProperty *prop = static_cast<WatchesProperty*>(event.GetProperty());
    SetValue(prop);
}

void WatchesDlg::OnPropertyChanging(wxPropertyGridEvent &event)
{
    if(event.GetProperty()->GetCount() > 0)
        event.Veto(true);
}

void WatchesDlg::OnPropertyLableEditBegin(wxPropertyGridEvent &event)
{
    wxPGProperty *prop = event.GetProperty();

    if(prop)
    {
        wxPGProperty *prop_parent = prop->GetParent();
        if(prop_parent && !prop_parent->IsRoot())
            event.Veto(true);
    }
}

void WatchesDlg::OnPropertyLableEditEnd(wxPropertyGridEvent &event)
{
    const wxString& label = m_grid->GetLabelEditor()->GetValue();
    RenameWatch(event.GetProperty(), label);
}

void WatchesDlg::OnIdle(wxIdleEvent &event)
{
    if (m_append_empty_watch)
    {
        wxPGProperty *new_prop = m_grid->Append(new WatchesProperty(wxEmptyString, wxEmptyString, NULL, false));
        m_grid->SelectProperty(new_prop, true);
        m_grid->Refresh();
        m_append_empty_watch = false;
    }
}

void WatchesDlg::OnPropertySelected(wxPropertyGridEvent &event)
{
    if (event.GetProperty() && event.GetPropertyLabel() == wxEmptyString)
        m_grid->BeginLabelEdit(0);
}

void WatchesDlg::DeleteProperty(WatchesProperty &prop)
{
    cbWatch *watch = prop.GetWatch();
    if (!watch)
        return;

    cbDebuggerPlugin *debugger = Manager::Get()->GetDebuggerManager()->GetDebuggerHavingWatch(watch);
    debugger->DeleteWatch(watch);

    wxPGProperty *parent = prop.GetParent();
    if(parent && parent->IsRoot())
    {
        for (WatchItems::iterator it = m_watches.begin(); it != m_watches.end(); ++it)
        {
            if (!it->property)
                continue;
            if (it->property == &prop)
            {
                m_watches.erase(it);
                break;
            }
        }
        prop.DeleteChildren();
        m_grid->DeleteProperty(&prop);
    }
}

void WatchesDlg::OnKeyDown(wxKeyEvent &event)
{
    wxPGProperty *prop = m_grid->GetSelection();
    WatchesProperty *watches_prop = static_cast<WatchesProperty*>(prop);

    // don't delete the watch when editing the value or the label
    if (m_grid->IsEditorFocused() || m_grid->GetLabelEditor())
        return;

    if (!prop || !prop->GetParent() || !prop->GetParent()->IsRoot())
        return;

    switch (event.GetKeyCode())
    {
        case WXK_DELETE:
            {
                wxPGProperty *prop_to_select = m_grid->GetNextSiblingProperty(watches_prop);

                DeleteProperty(*watches_prop);

                if (prop_to_select)
                    m_grid->SelectProperty(prop_to_select, false);
            }
            break;
        case WXK_INSERT:
            m_grid->BeginLabelEdit(0);
            break;
    }
}

void WatchesDlg::OnPropertyRightClick(wxPropertyGridEvent &event)
{
    wxMenu m;
    m.Append(idMenuRename, _("Rename"), _("Rename the watch"));
    m.Append(idMenuProperties, _("Properties"), _("Show the properties for the watch"));
    m.Append(idMenuDelete, _("Delete"), _("Delete the currently selected watch"));
    m.Append(idMenuDeleteAll, _("Delete All"), _("Delete all watches"));

    WatchesProperty *prop = dynamic_cast<WatchesProperty*>(event.GetProperty());
    cbDebuggerPlugin *activeDebugger = Manager::Get()->GetDebuggerManager()->GetActiveDebugger();
    if (activeDebugger && prop)
    {
        cbWatch *watch = prop->GetWatch();
        if (watch)
        {
            int disabled = cbDebuggerPlugin::WatchesDisabledMenuItems::Empty;
            activeDebugger->OnWatchesContextMenu(m, *watch, event.GetProperty(), disabled);

            if (disabled & cbDebuggerPlugin::WatchesDisabledMenuItems::Rename)
                m.Enable(idMenuRename, false);
            if (disabled & cbDebuggerPlugin::WatchesDisabledMenuItems::Properties)
                m.Enable(idMenuProperties, false);
            if (disabled & cbDebuggerPlugin::WatchesDisabledMenuItems::Delete)
                m.Enable(idMenuDelete, false);
            if (disabled & cbDebuggerPlugin::WatchesDisabledMenuItems::DeleteAll)
                m.Enable(idMenuDeleteAll, false);
        }
    }

    PopupMenu(&m);
}

void WatchesDlg::OnMenuRename(wxCommandEvent &event)
{
    if (!m_grid->GetLabelEditor())
    {
        m_grid->SetFocus();
        m_grid->BeginLabelEdit(0);
    }
}

void WatchesDlg::OnMenuProperties(wxCommandEvent &event)
{
    wxPGProperty *selected = m_grid->GetSelection();
    if (selected)
    {
        WatchesProperty *prop = static_cast<WatchesProperty*>(selected);
        cbWatch *watch = prop->GetWatch();
        if (watch)
        {
            cbDebuggerPlugin *debugger = Manager::Get()->GetDebuggerManager()->GetDebuggerHavingWatch(watch);
            if (debugger)
                debugger->ShowWatchProperties(watch);
        }
    }
}

void WatchesDlg::OnMenuDelete(wxCommandEvent &event)
{
    wxPGProperty *selected = m_grid->GetSelection();
    if (selected)
    {
        WatchesProperty *prop = static_cast<WatchesProperty*>(selected);
        DeleteProperty(*prop);
    }
}

void WatchesDlg::OnMenuDeleteAll(wxCommandEvent &event)
{
    for (WatchItems::iterator it = m_watches.begin(); it != m_watches.end(); ++it)
    {
        cbDebuggerPlugin *debugger = Manager::Get()->GetDebuggerManager()->GetDebuggerHavingWatch(it->watch);
        debugger->DeleteWatch(it->watch);
        m_grid->DeleteProperty(it->property);
    }
    m_watches.clear();
}

void WatchesDlg::RenameWatch(wxObject *prop, const wxString &newSymbol)
{
    cbDebuggerPlugin *plugin = Manager::Get()->GetDebuggerManager()->GetActiveDebugger();
    WatchesProperty *watchesProp = static_cast<WatchesProperty*>(prop);

    if (newSymbol == wxEmptyString)
        return;

    if (plugin && watchesProp)
    {
        // if the user have edited existing watch, we replace it.
        if (watchesProp->GetWatch())
        {
            cbWatch *old_watch = watchesProp->GetWatch();
            watchesProp->SetWatch(nullptr);
            plugin->DeleteWatch(old_watch);
            cbWatch::Pointer new_watch = plugin->AddWatch(newSymbol);
            watchesProp->SetWatch(new_watch.get());

            for (WatchItems::iterator it = m_watches.begin(); it != m_watches.end(); ++it)
            {
                if (it->property == watchesProp)
                    it->watch = new_watch.get();
            }
            watchesProp->SetExpanded(new_watch->IsExpanded());
            m_grid->Refresh();
        }
        else
        {
            WatchItem item;
            item.property = watchesProp;
            item.watch = plugin->AddWatch(newSymbol).get();
            watchesProp->SetWatch(item.watch);
            m_watches.push_back(item);
            watchesProp->SetExpanded(item.watch->IsExpanded());

            m_append_empty_watch = true;
        }
    }
}

//////////////////////////////////////////////////////////////////
//////////// ValueTooltip implementation /////////////////////////
//////////////////////////////////////////////////////////////////

IMPLEMENT_CLASS(ValueTooltip, wxPopupWindow)

BEGIN_EVENT_TABLE(ValueTooltip, wxPopupWindow)
    EVT_PG_ITEM_COLLAPSED(idTooltipGrid, ValueTooltip::OnCollapsed)
    EVT_PG_ITEM_EXPANDED(idTooltipGrid, ValueTooltip::OnExpanded)
    EVT_TIMER(idTooltipTimer, ValueTooltip::OnTimer)
END_EVENT_TABLE()

wxPGProperty* GetRealRoot(wxPropertyGrid *grid)
{
    wxPGProperty *property = grid->GetRoot();
    return property ? grid->GetFirstChild(property) : nullptr;
}

void GetColumnWidths(wxClientDC &dc, wxPropertyGrid *grid, wxPGProperty *root, int width[3])
{
    width[0] = width[1] = width[2] = 0;
    int minWidths[3] = { grid->GetState()->GetColumnMinWidth(0),
                         grid->GetState()->GetColumnMinWidth(1),
                         grid->GetState()->GetColumnMinWidth(2) };

    wxPropertyGridState *state = grid->GetState();
    for (unsigned ii = 0; ii < root->GetCount(); ++ii)
    {
        wxPGProperty* p = root->Item(ii);

        width[0] = std::max(width[0], state->GetColumnFullWidth(dc, p, 0));
        width[1] = std::max(width[1], state->GetColumnFullWidth(dc, p, 1));
        width[2] = std::max(width[2], state->GetColumnFullWidth(dc, p, 2));
    }
    for (unsigned ii = 0; ii < root->GetCount(); ++ii)
    {
        wxPGProperty* p = root->Item(ii);
        if (p->IsExpanded())
        {
            int w[3];
            GetColumnWidths(dc, grid, p, w);
            width[0] = std::max(width[0], w[0]);
            width[1] = std::max(width[1], w[1]);
            width[2] = std::max(width[2], w[2]);
        }
    }

    width[0] = std::max(width[0], minWidths[0]);
    width[1] = std::max(width[1], minWidths[1]);
    width[2] = std::max(width[2], minWidths[2]);
}

void GetColumnWidths(wxPropertyGrid *grid, wxPGProperty *root, int width[3])
{
    wxClientDC dc(grid);
    dc.SetFont(grid->GetFont());
    GetColumnWidths(dc, grid, root, width);
}

void SetMinSize(wxPropertyGrid *grid)
{
    wxPGProperty *p = GetRealRoot(grid);
    wxPGProperty *first = grid->GetFirstProperty();
    wxPGProperty *last = grid->GetLastProperty();
    wxRect rect = grid->GetPropertyRect(first, last);
    int height = rect.height + 2 * grid->GetVerticalSpacing();

    // add some height when the root item is collapsed,
    // this is needed to prevent the vertical scroll from showing
    if (!grid->IsPropertyExpanded(p))
        height += 2 * grid->GetVerticalSpacing();

    int width[3];
    GetColumnWidths(grid, grid->GetRoot(), width);
    rect.width = std::accumulate(width, width+3, 0);

    int minWidth = (wxSystemSettings::GetMetric(wxSYS_SCREEN_X, grid->GetParent())*3)/2;
    int minHeight = (wxSystemSettings::GetMetric(wxSYS_SCREEN_Y, grid->GetParent())*3)/2;

    wxSize size(std::min(minWidth, rect.width), std::min(minHeight, height));
    grid->SetMinSize(size);

    int proportions[3];
    proportions[0] = static_cast<int>(floor((double)width[0]/size.x*100.0+0.5));
    proportions[1] = static_cast<int>(floor((double)width[1]/size.x*100.0+0.5));
    proportions[2]= std::max(100 - proportions[0] - proportions[1], 0);
    grid->SetColumnProportion(0, proportions[0]);
    grid->SetColumnProportion(1, proportions[1]);
    grid->SetColumnProportion(2, proportions[2]);
    grid->ResetColumnSizes(true);
}

ValueTooltip::ValueTooltip(const cbWatch::Pointer &watch, wxWindow *parent) :
    wxPopupWindow(parent, wxBORDER_NONE|wxWANTS_CHARS),
    m_timer(this, idTooltipTimer),
    m_outsideCount(0),
    m_watch(watch)
{
    m_panel = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxSize(200, 200));
    m_grid = new wxPropertyGrid(m_panel, idTooltipGrid, wxDefaultPosition, wxSize(400,400), wxPG_SPLITTER_AUTO_CENTER);

    m_grid->SetExtraStyle(wxPG_EX_DISABLE_TLP_TRACKING /*| wxPG_EX_HELP_AS_TOOLTIPS*/);
    m_grid->SetDropTarget(new WatchesDropTarget);

    wxNativeFontInfo fontInfo;
    fontInfo.FromString(cbDebuggerCommonConfig::GetValueTooltipFont());
    wxFont font(fontInfo);
    m_grid->SetFont(font);

    m_grid->SetColumnCount(3);

    wxString symbol, value;
    m_watch->GetSymbol(symbol);
    m_watch->GetValue(value);
    m_watch->Expand(true);
    wxPGProperty *root = m_grid->Append(new WatchesProperty(symbol, value, m_watch.get(), true));
    m_watch->MarkAsChangedRecursive(false);
    ::UpdateWatch(m_grid, root, m_watch.get(), true);

    ::SetMinSize(m_grid);

    m_sizer = new wxBoxSizer( wxVERTICAL );
    m_sizer->Add(m_grid, 0, wxALL | wxEXPAND, 0);

    m_panel->SetAutoLayout(true);
    m_panel->SetSizer(m_sizer);
    m_sizer->Fit(m_panel);
    m_sizer->Fit(this);

    m_timer.Start(100);
}

ValueTooltip::~ValueTooltip()
{
    ClearWatch();
}

void ValueTooltip::ClearWatch()
{
    if (m_watch)
    {
        cbDebuggerPlugin *plugin = Manager::Get()->GetDebuggerManager()->GetDebuggerHavingWatch(m_watch.get());
        if (plugin)
            plugin->DeleteWatch(m_watch.get());
        m_watch.reset();
    }
}

void ValueTooltip::Dismiss()
{
    Hide();
    m_timer.Stop();
    ClearWatch();
}

void ValueTooltip::OnDismiss()
{
    ClearWatch();
}

void ValueTooltip::Fit()
{
    ::SetMinSize(m_grid);
    m_sizer->Fit(m_panel);
    wxPoint pos = GetScreenPosition();
    wxSize size = m_panel->GetScreenRect().GetSize();
    SetSize(pos.x, pos.y, size.x, size.y);
}

void ValueTooltip::OnCollapsed(wxPropertyGridEvent &event)
{
    Fit();
}

void ValueTooltip::OnExpanded(wxPropertyGridEvent &event)
{
    Fit();
}

void ValueTooltip::OnTimer(wxTimerEvent &event)
{
    if (!wxTheApp->IsActive())
        Dismiss();

    wxPoint mouse = wxGetMousePosition();
    wxRect rect = GetScreenRect();
    rect.Inflate(5);

    if (!rect.Contains(mouse))
    {
        if (++m_outsideCount > 5)
            Dismiss();
    }
    else
        m_outsideCount = 0;
}
