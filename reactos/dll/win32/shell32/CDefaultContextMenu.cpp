/*
 * PROJECT:     shell32
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        dll/win32/shell32/shv_item_new.c
 * PURPOSE:     provides default context menu implementation
 * PROGRAMMERS: Johannes Anderwald (johannes.anderwald@reactos.org)
 */

#include "precomp.h"

extern "C"
{
    //fixme: this isn't in wine's shlwapi header, and the definition doesnt match the
    // windows headers. When wine's header and lib are fixed this can be removed.
    DWORD WINAPI SHAnsiToUnicode(LPCSTR lpSrcStr, LPWSTR lpDstStr, int iLen);
};

WINE_DEFAULT_DEBUG_CHANNEL(dmenu);

typedef struct _DynamicShellEntry_
{
    UINT iIdCmdFirst;
    UINT NumIds;
    CLSID ClassID;
    IContextMenu *pCM;
    struct _DynamicShellEntry_ *pNext;
} DynamicShellEntry, *PDynamicShellEntry;

typedef struct _StaticShellEntry_
{
    LPWSTR szVerb;
    HKEY hkClass;
    struct _StaticShellEntry_ *pNext;
} StaticShellEntry, *PStaticShellEntry;


//
// verbs for InvokeCommandInfo
//
struct _StaticInvokeCommandMap_
{
    LPCSTR szStringVerb;
    UINT IntVerb;
} g_StaticInvokeCmdMap[] = 
{
    { "RunAs", 0 },  // Unimplemented
    { "Print", 0 },  // Unimplemented
    { "Preview", 0 }, // Unimplemented
    { "Open", FCIDM_SHVIEW_OPEN },
    { CMDSTR_NEWFOLDERA, FCIDM_SHVIEW_NEWFOLDER },
    { CMDSTR_VIEWLISTA, FCIDM_SHVIEW_LISTVIEW },
    { CMDSTR_VIEWDETAILSA, FCIDM_SHVIEW_REPORTVIEW }
};


class CDefaultContextMenu :
    public CComObjectRootEx<CComMultiThreadModelNoCS>,
    public IContextMenu3,
    public IObjectWithSite
{
    private:
        CComPtr<IUnknown> m_site;
        CComPtr<IShellFolder> m_psf;
        UINT m_cidl;
        PCUITEMID_CHILD_ARRAY m_apidl;
        CComPtr<IDataObject> m_pDataObj;
        HKEY* m_aKeys;
        UINT m_cKeys;
        PIDLIST_ABSOLUTE m_pidlFolder;
        DWORD m_bGroupPolicyActive;
        PDynamicShellEntry m_pDynamicEntries; /* first dynamic shell extension entry */
        UINT m_iIdSHEFirst; /* first used id */
        UINT m_iIdSHELast; /* last used id */
        PStaticShellEntry m_pStaticEntries; /* first static shell extension entry */
        UINT m_iIdSCMFirst; /* first static used id */
        UINT m_iIdSCMLast; /* last static used id */

        void AddStaticEntry(const HKEY hkeyClass, const WCHAR *szVerb);
        void AddStaticEntriesForKey(HKEY hKey);
        BOOL IsShellExtensionAlreadyLoaded(const CLSID *pclsid);
        HRESULT LoadDynamicContextMenuHandler(HKEY hKey, const CLSID *pclsid);
        BOOL EnumerateDynamicContextHandlerForKey(HKEY hRootKey);
        UINT InsertMenuItemsOfDynamicContextMenuExtension(HMENU hMenu, UINT IndexMenu, UINT idCmdFirst, UINT idCmdLast);
        UINT BuildBackgroundContextMenu(HMENU hMenu, UINT iIdCmdFirst, UINT iIdCmdLast, UINT uFlags);
        UINT AddStaticContextMenusToMenu(HMENU hMenu, UINT IndexMenu);
        UINT BuildShellItemContextMenu(HMENU hMenu, UINT iIdCmdFirst, UINT iIdCmdLast, UINT uFlags);
        HRESULT NotifyShellViewWindow(LPCMINVOKECOMMANDINFO lpcmi, BOOL bRefresh);
        HRESULT DoPaste(LPCMINVOKECOMMANDINFO lpcmi, BOOL bLink);
        HRESULT DoOpenOrExplore(LPCMINVOKECOMMANDINFO lpcmi);
        HRESULT DoCreateLink(LPCMINVOKECOMMANDINFO lpcmi);
        HRESULT DoRefresh(LPCMINVOKECOMMANDINFO lpcmi);
        HRESULT DoDelete(LPCMINVOKECOMMANDINFO lpcmi);
        HRESULT DoCopyOrCut(LPCMINVOKECOMMANDINFO lpcmi, BOOL bCopy);
        HRESULT DoRename(LPCMINVOKECOMMANDINFO lpcmi);
        HRESULT DoProperties(LPCMINVOKECOMMANDINFO lpcmi);
        HRESULT DoFormat(LPCMINVOKECOMMANDINFO lpcmi);
        HRESULT DoCreateNewFolder(LPCMINVOKECOMMANDINFO lpici);
        HRESULT DoDynamicShellExtensions(LPCMINVOKECOMMANDINFO lpcmi);
        HRESULT DoStaticShellExtensions(LPCMINVOKECOMMANDINFO lpcmi);
        DWORD BrowserFlagsFromVerb(LPCMINVOKECOMMANDINFO lpcmi, PStaticShellEntry pEntry);
        HRESULT TryToBrowse(LPCMINVOKECOMMANDINFO lpcmi, LPCITEMIDLIST pidl, DWORD wFlags);
        HRESULT InvokePidl(LPCMINVOKECOMMANDINFO lpcmi, LPCITEMIDLIST pidl, PStaticShellEntry pEntry);
        PDynamicShellEntry GetDynamicEntry(UINT idCmd);
        BOOL MapVerbToCmdId(PVOID Verb, PUINT idCmd, BOOL IsUnicode);

    public:
        CDefaultContextMenu();
        ~CDefaultContextMenu();
        HRESULT WINAPI Initialize(const DEFCONTEXTMENU *pdcm);

        // IContextMenu
        virtual HRESULT WINAPI QueryContextMenu(HMENU hMenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags);
        virtual HRESULT WINAPI InvokeCommand(LPCMINVOKECOMMANDINFO lpcmi);
        virtual HRESULT WINAPI GetCommandString(UINT_PTR idCommand, UINT uFlags, UINT *lpReserved, LPSTR lpszName, UINT uMaxNameLen);

        // IContextMenu2
        virtual HRESULT WINAPI HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam);

        // IContextMenu3
        virtual HRESULT WINAPI HandleMenuMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *plResult);

        // IObjectWithSite
        virtual HRESULT STDMETHODCALLTYPE SetSite(IUnknown *pUnkSite);
        virtual HRESULT STDMETHODCALLTYPE GetSite(REFIID riid, void **ppvSite);

        BEGIN_COM_MAP(CDefaultContextMenu)
        COM_INTERFACE_ENTRY_IID(IID_IContextMenu, IContextMenu)
        COM_INTERFACE_ENTRY_IID(IID_IContextMenu2, IContextMenu2)
        COM_INTERFACE_ENTRY_IID(IID_IContextMenu3, IContextMenu3)
        COM_INTERFACE_ENTRY_IID(IID_IObjectWithSite, IObjectWithSite)
        END_COM_MAP()
};

CDefaultContextMenu::CDefaultContextMenu() :
    m_psf(NULL),
    m_cidl(0),
    m_apidl(NULL),
    m_pDataObj(NULL),
    m_aKeys(NULL),
    m_cKeys(NULL),
    m_pidlFolder(NULL),
    m_bGroupPolicyActive(0),
    m_pDynamicEntries(NULL),
    m_iIdSHEFirst(0),
    m_iIdSHELast(0),
    m_pStaticEntries(NULL),
    m_iIdSCMFirst(0),
    m_iIdSCMLast(0)
{
}

CDefaultContextMenu::~CDefaultContextMenu()
{
    /* Free dynamic shell extension entries */
    PDynamicShellEntry pDynamicEntry = m_pDynamicEntries, pNextDynamic;
    while (pDynamicEntry)
    {
        pNextDynamic = pDynamicEntry->pNext;
        pDynamicEntry->pCM->Release();
        HeapFree(GetProcessHeap(), 0, pDynamicEntry);
        pDynamicEntry = pNextDynamic;
    }

    /* Free static shell extension entries */
    PStaticShellEntry pStaticEntry = m_pStaticEntries, pNextStatic;
    while (pStaticEntry)
    {
        pNextStatic = pStaticEntry->pNext;
        HeapFree(GetProcessHeap(), 0, pStaticEntry->szVerb);
        HeapFree(GetProcessHeap(), 0, pStaticEntry);
        pStaticEntry = pNextStatic;
    }

    for (UINT i = 0; i < m_cKeys; i++)
        RegCloseKey(m_aKeys[i]);
    HeapFree(GetProcessHeap(), 0, m_aKeys);

    if (m_pidlFolder)
        CoTaskMemFree(m_pidlFolder);
    _ILFreeaPidl(const_cast<PITEMID_CHILD *>(m_apidl), m_cidl);
}

HRESULT WINAPI CDefaultContextMenu::Initialize(const DEFCONTEXTMENU *pdcm)
{
    TRACE("cidl %u\n", pdcm->cidl);

    m_cidl = pdcm->cidl;
    m_apidl = const_cast<PCUITEMID_CHILD_ARRAY>(_ILCopyaPidl(pdcm->apidl, m_cidl));
    if (m_cidl && !m_apidl)
        return E_OUTOFMEMORY;
    m_psf = pdcm->psf;

    m_cKeys = pdcm->cKeys;
    if (pdcm->cKeys)
    {
        m_aKeys = (HKEY*)HeapAlloc(GetProcessHeap(), 0, sizeof(HKEY) * pdcm->cKeys);
        if (!m_aKeys)
            return E_OUTOFMEMORY;
        memcpy(m_aKeys, pdcm->aKeys, sizeof(HKEY) * pdcm->cKeys);
    }

    m_psf->GetUIObjectOf(pdcm->hwnd, m_cidl, m_apidl, IID_NULL_PPV_ARG(IDataObject, &m_pDataObj));

    if (pdcm->pidlFolder)
    {
        m_pidlFolder = ILClone(pdcm->pidlFolder);
    }
    else
    {
        CComPtr<IPersistFolder2> pf = NULL;
        if (SUCCEEDED(m_psf->QueryInterface(IID_PPV_ARG(IPersistFolder2, &pf))))
        {
            if (FAILED(pf->GetCurFolder(reinterpret_cast<LPITEMIDLIST*>(&m_pidlFolder))))
                ERR("GetCurFolder failed\n");
        }
        TRACE("pidlFolder %p\n", m_pidlFolder);
    }

    return S_OK;
}

void CDefaultContextMenu::AddStaticEntry(const HKEY hkeyClass, const WCHAR *szVerb)
{
    PStaticShellEntry pEntry = m_pStaticEntries, pLastEntry = NULL;
    while(pEntry)
    {
        if (!wcsicmp(pEntry->szVerb, szVerb))
        {
            /* entry already exists */
            return;
        }
        pLastEntry = pEntry;
        pEntry = pEntry->pNext;
    }

    TRACE("adding verb %s\n", debugstr_w(szVerb));

    pEntry = (StaticShellEntry *)HeapAlloc(GetProcessHeap(), 0, sizeof(StaticShellEntry));
    if (pEntry)
    {
        pEntry->pNext = NULL;
        pEntry->szVerb = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, (wcslen(szVerb) + 1) * sizeof(WCHAR));
        if (pEntry->szVerb)
            wcscpy(pEntry->szVerb, szVerb);
        pEntry->hkClass = hkeyClass;
    }

    if (!wcsicmp(szVerb, L"open"))
    {
        /* open verb is always inserted in front */
        pEntry->pNext = m_pStaticEntries;
        m_pStaticEntries = pEntry;
    }
    else if (pLastEntry)
        pLastEntry->pNext = pEntry;
    else
        m_pStaticEntries = pEntry;
}

void CDefaultContextMenu::AddStaticEntriesForKey(HKEY hKey)
{
    WCHAR wszName[40];
    DWORD cchName, dwIndex = 0;
    HKEY hShellKey;

    LRESULT lres = RegOpenKeyExW(hKey, L"shell", 0, KEY_READ, &hShellKey);
    if (lres != STATUS_SUCCESS)
        return;

    while(TRUE)
    {
        cchName = _countof(wszName);
        if (RegEnumKeyExW(hShellKey, dwIndex++, wszName, &cchName, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
            break;

        AddStaticEntry(hKey, wszName);
    }

    RegCloseKey(hShellKey);
}

static
BOOL
HasClipboardData()
{
    BOOL bRet = FALSE;
    CComPtr<IDataObject> pDataObj;

    if (SUCCEEDED(OleGetClipboard(&pDataObj)))
    {
        STGMEDIUM medium;
        FORMATETC formatetc;

        TRACE("pDataObj=%p\n", pDataObj.p);

        /* Set the FORMATETC structure*/
        InitFormatEtc(formatetc, RegisterClipboardFormatW(CFSTR_SHELLIDLIST), TYMED_HGLOBAL);
        if (SUCCEEDED(pDataObj->GetData(&formatetc, &medium)))
        {
            bRet = TRUE;
            ReleaseStgMedium(&medium);
        }
    }

    return bRet;
}

static
VOID
DisablePasteOptions(HMENU hMenu)
{
    MENUITEMINFOW mii;

    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STATE;
    mii.fState = MFS_DISABLED;

    SetMenuItemInfoW(hMenu, FCIDM_SHVIEW_INSERT, FALSE, &mii);
    SetMenuItemInfoW(hMenu, FCIDM_SHVIEW_INSERTLINK, FALSE, &mii);
}

BOOL
CDefaultContextMenu::IsShellExtensionAlreadyLoaded(const CLSID *pclsid)
{
    PDynamicShellEntry pEntry = m_pDynamicEntries;

    while (pEntry)
    {
        if (!memcmp(&pEntry->ClassID, pclsid, sizeof(CLSID)))
            return TRUE;
        pEntry = pEntry->pNext;
    }

    return FALSE;
}

HRESULT
CDefaultContextMenu::LoadDynamicContextMenuHandler(HKEY hKey, const CLSID *pclsid)
{
    HRESULT hr;

    TRACE("LoadDynamicContextMenuHandler entered with This %p hKey %p pclsid %s\n", this, hKey, wine_dbgstr_guid(pclsid));

    if (IsShellExtensionAlreadyLoaded(pclsid))
        return S_OK;

    CComPtr<IContextMenu> pcm;
    hr = SHCoCreateInstance(NULL, pclsid, NULL, IID_PPV_ARG(IContextMenu, &pcm));
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    CComPtr<IShellExtInit> pExtInit;
    hr = pcm->QueryInterface(IID_PPV_ARG(IShellExtInit, &pExtInit));
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    hr = pExtInit->Initialize(m_pidlFolder, m_pDataObj, hKey);
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    PDynamicShellEntry pEntry = (DynamicShellEntry *)HeapAlloc(GetProcessHeap(), 0, sizeof(DynamicShellEntry));
    if (!pEntry)
    {
        return E_OUTOFMEMORY;
    }

    pEntry->iIdCmdFirst = 0;
    pEntry->pNext = NULL;
    pEntry->NumIds = 0;
    pEntry->pCM = pcm.Detach();
    memcpy(&pEntry->ClassID, pclsid, sizeof(CLSID));

    if (m_pDynamicEntries)
    {
        PDynamicShellEntry pLastEntry = m_pDynamicEntries;

        while (pLastEntry->pNext)
            pLastEntry = pLastEntry->pNext;

        pLastEntry->pNext = pEntry;
    }
    else
        m_pDynamicEntries = pEntry;

    return S_OK;
}

BOOL
CDefaultContextMenu::EnumerateDynamicContextHandlerForKey(HKEY hRootKey)
{

    WCHAR wszName[MAX_PATH], wszBuf[MAX_PATH], *pwszClsid;
    DWORD cchName;
    HRESULT hr;
    HKEY hKey;

    if (RegOpenKeyExW(hRootKey, L"shellex\\ContextMenuHandlers", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
    {
        TRACE("RegOpenKeyExW failed\n");
        return FALSE;
    }

    DWORD dwIndex = 0;
    while (TRUE)
    {
        cchName = _countof(wszName);
        if (RegEnumKeyExW(hKey, dwIndex++, wszName, &cchName, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
            break;

        /* Key name or key value is CLSID */
        CLSID clsid;
        hr = CLSIDFromString(wszName, &clsid);
        if (hr == S_OK)
            pwszClsid = wszName;
        else
        {
            DWORD cchBuf = _countof(wszBuf);
            if (RegGetValueW(hKey, wszName, NULL, RRF_RT_REG_SZ, NULL, wszBuf, &cchBuf) == ERROR_SUCCESS)
                hr = CLSIDFromString(wszBuf, &clsid);
            pwszClsid = wszBuf;
        }
        if (SUCCEEDED(hr))
        {
            if (m_bGroupPolicyActive)
            {
                if (RegGetValueW(HKEY_LOCAL_MACHINE,
                                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
                                 pwszClsid,
                                 RRF_RT_REG_SZ,
                                 NULL,
                                 NULL,
                                 NULL) == ERROR_SUCCESS)
                {
                    LoadDynamicContextMenuHandler(hKey, &clsid);
                }
            }
            else
                LoadDynamicContextMenuHandler(hKey, &clsid);
        }
    }

    RegCloseKey(hKey);
    return TRUE;
}

UINT
CDefaultContextMenu::InsertMenuItemsOfDynamicContextMenuExtension(HMENU hMenu, UINT IndexMenu, UINT idCmdFirst, UINT idCmdLast)
{
    if (!m_pDynamicEntries)
    {
        m_iIdSHEFirst = 0;
        m_iIdSHELast = 0;
        return IndexMenu;
    }

    PDynamicShellEntry pEntry = m_pDynamicEntries;
    idCmdFirst = 0x5000;
    idCmdLast =  0x6000;
    m_iIdSHEFirst = idCmdFirst;
    do
    {
        HRESULT hr = pEntry->pCM->QueryContextMenu(hMenu, IndexMenu++, idCmdFirst, idCmdLast, CMF_NORMAL);
        if (SUCCEEDED(hr))
        {
            pEntry->iIdCmdFirst = idCmdFirst;
            pEntry->NumIds = LOWORD(hr);
            IndexMenu += pEntry->NumIds;
            idCmdFirst += pEntry->NumIds + 0x10;
        }
        TRACE("pEntry %p hr %x contextmenu %p cmdfirst %x num ids %x\n", pEntry, hr, pEntry->pCM, pEntry->iIdCmdFirst, pEntry->NumIds);
        pEntry = pEntry->pNext;
    } while (pEntry);

    m_iIdSHELast = idCmdFirst;
    TRACE("SH_LoadContextMenuHandlers first %x last %x\n", m_iIdSHEFirst, m_iIdSHELast);
    return IndexMenu;
}

UINT
CDefaultContextMenu::BuildBackgroundContextMenu(
    HMENU hMenu,
    UINT iIdCmdFirst,
    UINT iIdCmdLast,
    UINT uFlags)
{
    UINT IndexMenu = 0;
    HMENU hSubMenu;

    TRACE("BuildBackgroundContextMenu entered\n");

    SFGAOF rfg = SFGAO_FILESYSTEM | SFGAO_FOLDER;
    HRESULT hr = m_psf->GetAttributesOf(0, NULL, &rfg);
    if (FAILED(hr))
    {
        ERR("GetAttributesOf failed: %x\n", hr);
        rfg = 0;
    }

    hSubMenu = LoadMenuW(shell32_hInstance, L"MENU_002");
    if (hSubMenu)
    {
        /* view option is only available in browsing mode */
        if (_ILIsDesktop(m_pidlFolder))
            DeleteMenu(hSubMenu, FCIDM_SHVIEW_VIEW, MF_BYCOMMAND);

        /* merge general background context menu in */
        iIdCmdFirst = Shell_MergeMenus(hMenu, GetSubMenu(hSubMenu, 0), IndexMenu, 0, 0xFFFF, MM_DONTREMOVESEPS | MM_SUBMENUSHAVEIDS) + 1;
        DestroyMenu(hSubMenu);
    }

    if (!HasClipboardData())
    {
        TRACE("disabling paste options\n");
        DisablePasteOptions(hMenu);
    }

    /* Directory is progid of filesystem folders only */
    if ((rfg & (SFGAO_FILESYSTEM|SFGAO_FOLDER)) == (SFGAO_FILESYSTEM|SFGAO_FOLDER))
    {
        /* Load context menu handlers */
        TRACE("Add background handlers: %p\n", m_pidlFolder);
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CLASSES_ROOT, L"Directory\\Background", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            EnumerateDynamicContextHandlerForKey(hKey);
            RegCloseKey(hKey);
        }

        if (InsertMenuItemsOfDynamicContextMenuExtension(hMenu, GetMenuItemCount(hMenu) - 1, iIdCmdFirst, iIdCmdLast))
        {
            /* seperate dynamic context menu items */
            _InsertMenuItemW(hMenu, GetMenuItemCount(hMenu) - 1, TRUE, -1, MFT_SEPARATOR, NULL, MFS_ENABLED);
        }
    }

    return iIdCmdLast;
}

UINT
CDefaultContextMenu::AddStaticContextMenusToMenu(
    HMENU hMenu,
    UINT IndexMenu)
{
    MENUITEMINFOW mii;
    UINT idResource;
    WCHAR wszVerb[40];
    UINT fState;

    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_ID | MIIM_TYPE | MIIM_STATE | MIIM_DATA;
    mii.fType = MFT_STRING;
    mii.wID = 0x4000;
    mii.dwTypeData = NULL;
    m_iIdSCMFirst = mii.wID;

    PStaticShellEntry pEntry = m_pStaticEntries;

    while (pEntry)
    {
        fState = MFS_ENABLED;
        mii.dwTypeData = NULL;

        /* set first entry as default */
        if (pEntry == m_pStaticEntries)
            fState |= MFS_DEFAULT;

        if (!wcsicmp(pEntry->szVerb, L"open"))
        {
            /* override default when open verb is found */
            fState |= MFS_DEFAULT;
            idResource = IDS_OPEN_VERB;
        }
        else if (!wcsicmp(pEntry->szVerb, L"explore"))
            idResource = IDS_EXPLORE_VERB;
        else if (!wcsicmp(pEntry->szVerb, L"runas"))
            idResource = IDS_RUNAS_VERB;
        else if (!wcsicmp(pEntry->szVerb, L"edit"))
            idResource = IDS_EDIT_VERB;
        else if (!wcsicmp(pEntry->szVerb, L"find"))
            idResource = IDS_FIND_VERB;
        else if (!wcsicmp(pEntry->szVerb, L"print"))
            idResource = IDS_PRINT_VERB;
        else if (!wcsicmp(pEntry->szVerb, L"printto"))
        {
            pEntry = pEntry->pNext;
            continue;
        }
        else
            idResource = 0;

        /* By default use verb for menu item name */
        mii.dwTypeData = pEntry->szVerb;

        if (idResource > 0)
        {
            if (LoadStringW(shell32_hInstance, idResource, wszVerb, _countof(wszVerb)))
                mii.dwTypeData = wszVerb; /* use translated verb */
            else
                ERR("Failed to load string\n");
        }
        else
        {
            WCHAR wszKey[256];
            HRESULT hr = StringCbPrintfW(wszKey, sizeof(wszKey), L"shell\\%s", pEntry->szVerb);

            if (SUCCEEDED(hr))
            {
                HKEY hkVerb;
                DWORD cbVerb = sizeof(wszVerb);
                LONG res = RegOpenKeyW(pEntry->hkClass, wszKey, &hkVerb);
                if (res == ERROR_SUCCESS)
                {
                    res = RegLoadMUIStringW(hkVerb, 
                                            NULL,
                                            wszVerb,
                                            cbVerb,
                                            NULL,
                                            0,
                                            NULL);
                    if (res == ERROR_SUCCESS)
                    {
                        /* use description for the menu entry */
                        mii.dwTypeData = wszVerb; 
                    }

                    RegCloseKey(hkVerb);
                }
            }
        }

        mii.cch = wcslen(mii.dwTypeData);
        mii.fState = fState;
        InsertMenuItemW(hMenu, IndexMenu++, TRUE, &mii);

        mii.wID++;
        pEntry = pEntry->pNext;
    }

    m_iIdSCMLast = mii.wID - 1;
    return IndexMenu;
}

void WINAPI _InsertMenuItemW(
    HMENU hMenu,
    UINT indexMenu,
    BOOL fByPosition,
    UINT wID,
    UINT fType,
    LPCWSTR dwTypeData,
    UINT fState)
{
    MENUITEMINFOW mii;
    WCHAR wszText[100];

    ZeroMemory(&mii, sizeof(mii));
    mii.cbSize = sizeof(mii);
    if (fType == MFT_SEPARATOR)
        mii.fMask = MIIM_ID | MIIM_TYPE;
    else if (fType == MFT_STRING)
    {
        mii.fMask = MIIM_ID | MIIM_TYPE | MIIM_STATE;
        if ((ULONG_PTR)HIWORD((ULONG_PTR)dwTypeData) == 0)
        {
            if (LoadStringW(shell32_hInstance, LOWORD((ULONG_PTR)dwTypeData), wszText, _countof(wszText)))
                mii.dwTypeData = wszText;
            else
            {
                ERR("failed to load string %p\n", dwTypeData);
                return;
            }
        }
        else
            mii.dwTypeData = (LPWSTR)dwTypeData;
        mii.fState = fState;
    }

    mii.wID = wID;
    mii.fType = fType;
    InsertMenuItemW(hMenu, indexMenu, fByPosition, &mii);
}

UINT
CDefaultContextMenu::BuildShellItemContextMenu(
    HMENU hMenu,
    UINT iIdCmdFirst,
    UINT iIdCmdLast,
    UINT uFlags)
{
    HRESULT hr;

    TRACE("BuildShellItemContextMenu entered\n");
    ASSERT(m_cidl >= 1);

    for (UINT i = 0; i < m_cKeys; i++)
    {
        AddStaticEntriesForKey(m_aKeys[i]);
        EnumerateDynamicContextHandlerForKey(m_aKeys[i]);
    }

    /* add static actions */
    SFGAOF rfg = SFGAO_BROWSABLE | SFGAO_CANCOPY | SFGAO_CANLINK | SFGAO_CANMOVE | SFGAO_CANDELETE | SFGAO_CANRENAME | SFGAO_HASPROPSHEET | SFGAO_FILESYSTEM | SFGAO_FOLDER;
    hr = m_psf->GetAttributesOf(m_cidl, m_apidl, &rfg);
    if (FAILED(hr))
    {
        ERR("GetAttributesOf failed: %x\n", hr);
        rfg = 0;
    }

    /* add static context menu handlers */
    UINT IndexMenu = AddStaticContextMenusToMenu(hMenu, 0);

    /* now process dynamic context menu handlers */
    BOOL bAddSep = FALSE;
    IndexMenu = InsertMenuItemsOfDynamicContextMenuExtension(hMenu, IndexMenu, iIdCmdFirst, iIdCmdLast);
    TRACE("IndexMenu %d\n", IndexMenu);

    if (_ILIsDrive(m_apidl[0]))
    {
        char szDrive[8] = {0};
        DWORD dwFlags;

        _ILGetDrive(m_apidl[0], szDrive, sizeof(szDrive));
        if (GetVolumeInformationA(szDrive, NULL, 0, NULL, NULL, &dwFlags, NULL, 0))
        {
            /* Disable format if read only */
            if (!(dwFlags & FILE_READ_ONLY_VOLUME))
            {
                _InsertMenuItemW(hMenu, IndexMenu++, TRUE, 0, MFT_SEPARATOR, NULL, 0);
                _InsertMenuItemW(hMenu, IndexMenu++, TRUE, 0x7ABC, MFT_STRING, MAKEINTRESOURCEW(IDS_FORMATDRIVE), MFS_ENABLED);
                bAddSep = TRUE;
            }
        }
    }

    BOOL bClipboardData = (HasClipboardData() && (rfg & SFGAO_FILESYSTEM));
    if (rfg & (SFGAO_CANCOPY | SFGAO_CANMOVE) || bClipboardData)
    {
        _InsertMenuItemW(hMenu, IndexMenu++, TRUE, 0, MFT_SEPARATOR, NULL, 0);
        if (rfg & SFGAO_CANMOVE)
            _InsertMenuItemW(hMenu, IndexMenu++, TRUE, FCIDM_SHVIEW_CUT, MFT_STRING, MAKEINTRESOURCEW(IDS_CUT), MFS_ENABLED);
        if (rfg & SFGAO_CANCOPY)
            _InsertMenuItemW(hMenu, IndexMenu++, TRUE, FCIDM_SHVIEW_COPY, MFT_STRING, MAKEINTRESOURCEW(IDS_COPY), MFS_ENABLED);
        if (bClipboardData)
            _InsertMenuItemW(hMenu, IndexMenu++, TRUE, FCIDM_SHVIEW_INSERT, MFT_STRING, MAKEINTRESOURCEW(IDS_PASTE), MFS_ENABLED);

        bAddSep = TRUE;
    }

    if (rfg & SFGAO_CANLINK)
    {
        bAddSep = FALSE;
        _InsertMenuItemW(hMenu, IndexMenu++, TRUE, 0, MFT_SEPARATOR, NULL, 0);
        _InsertMenuItemW(hMenu, IndexMenu++, TRUE, FCIDM_SHVIEW_CREATELINK, MFT_STRING, MAKEINTRESOURCEW(IDS_CREATELINK), MFS_ENABLED);
    }

    if (rfg & SFGAO_CANDELETE)
    {
        if (bAddSep)
        {
            bAddSep = FALSE;
            _InsertMenuItemW(hMenu, IndexMenu++, TRUE, 0, MFT_SEPARATOR, NULL, 0);
        }
        _InsertMenuItemW(hMenu, IndexMenu++, TRUE, FCIDM_SHVIEW_DELETE, MFT_STRING, MAKEINTRESOURCEW(IDS_DELETE), MFS_ENABLED);
    }

    if (rfg & SFGAO_CANRENAME)
    {
        if (bAddSep)
        {
            _InsertMenuItemW(hMenu, IndexMenu++, TRUE, 0, MFT_SEPARATOR, NULL, 0);
        }
        _InsertMenuItemW(hMenu, IndexMenu++, TRUE, FCIDM_SHVIEW_RENAME, MFT_STRING, MAKEINTRESOURCEW(IDS_RENAME), MFS_ENABLED);
        bAddSep = TRUE;
    }

    if (rfg & SFGAO_HASPROPSHEET)
    {
        _InsertMenuItemW(hMenu, IndexMenu++, TRUE, 0, MFT_SEPARATOR, NULL, 0);
        _InsertMenuItemW(hMenu, IndexMenu++, TRUE, FCIDM_SHVIEW_PROPERTIES, MFT_STRING, MAKEINTRESOURCEW(IDS_PROPERTIES), MFS_ENABLED);
    }

    return iIdCmdLast;
}

HRESULT
WINAPI
CDefaultContextMenu::QueryContextMenu(
    HMENU hMenu,
    UINT IndexMenu,
    UINT idCmdFirst,
    UINT idCmdLast,
    UINT uFlags)
{
    if (m_cidl)
        idCmdFirst = BuildShellItemContextMenu(hMenu, idCmdFirst, idCmdLast, uFlags);
    else
        idCmdFirst = BuildBackgroundContextMenu(hMenu, idCmdFirst, idCmdLast, uFlags);

    return S_OK;
}

HRESULT
CDefaultContextMenu::NotifyShellViewWindow(LPCMINVOKECOMMANDINFO lpcmi, BOOL bRefresh)
{
    if (!m_site)
        return E_FAIL;

    /* Get a pointer to the shell browser */
    CComPtr<IShellView> psv;
    HRESULT hr = IUnknown_QueryService(m_site, SID_IFolderView, IID_PPV_ARG(IShellView, &psv));
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    HWND hwndSV = NULL;
    if (SUCCEEDED(psv->GetWindow(&hwndSV)))
        SendMessageW(hwndSV, WM_COMMAND, MAKEWPARAM(LOWORD(lpcmi->lpVerb), 0), 0);
    return S_OK;
}


HRESULT CDefaultContextMenu::DoRefresh(LPCMINVOKECOMMANDINFO lpcmi)
{
    if (!m_site)
        return E_FAIL;

    /* Get a pointer to the shell view */
    CComPtr<IShellView> psv;
    HRESULT hr = IUnknown_QueryService(m_site, SID_IFolderView, IID_PPV_ARG(IShellView, &psv));
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    return psv->Refresh();
}

HRESULT CDefaultContextMenu::DoPaste(LPCMINVOKECOMMANDINFO lpcmi, BOOL bLink)
{
    HRESULT hr;

    CComPtr<IDataObject> pda;
    hr = OleGetClipboard(&pda);
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    FORMATETC formatetc2;
    STGMEDIUM medium2;
    InitFormatEtc(formatetc2, RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT), TYMED_HGLOBAL);

    DWORD dwKey= 0;

    if (SUCCEEDED(pda->GetData(&formatetc2, &medium2)))
    {
        DWORD * pdwFlag = (DWORD*)GlobalLock(medium2.hGlobal);
        if (pdwFlag)
        {
            if (*pdwFlag == DROPEFFECT_COPY)
                dwKey = MK_CONTROL;
            else
                dwKey = MK_SHIFT;
        }
        else {
            ERR("No drop effect obtained");
        }
        GlobalUnlock(medium2.hGlobal);
    }

    if (bLink)
    {
        dwKey = MK_CONTROL|MK_SHIFT;
    }

    CComPtr<IDropTarget> pdrop;
    if (m_cidl)
        hr = m_psf->GetUIObjectOf(NULL, 1, &m_apidl[0], IID_NULL_PPV_ARG(IDropTarget, &pdrop));
    else
        hr = m_psf->CreateViewObject(NULL, IID_PPV_ARG(IDropTarget, &pdrop));

    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    SHSimulateDrop(pdrop, pda, dwKey, NULL, NULL);

    TRACE("CP result %x\n", hr);
    return S_OK;
}

HRESULT
CDefaultContextMenu::DoOpenOrExplore(LPCMINVOKECOMMANDINFO lpcmi)
{
    UNIMPLEMENTED;
    return E_FAIL;
}

HRESULT CDefaultContextMenu::DoCreateLink(LPCMINVOKECOMMANDINFO lpcmi)
{
    if (!m_cidl || !m_pDataObj)
        return E_FAIL;

    CComPtr<IDropTarget> pDT;
    HRESULT hr = m_psf->CreateViewObject(NULL, IID_PPV_ARG(IDropTarget, &pDT));
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    SHSimulateDrop(pDT, m_pDataObj, MK_CONTROL|MK_SHIFT, NULL, NULL);

    return S_OK;
}

HRESULT CDefaultContextMenu::DoDelete(LPCMINVOKECOMMANDINFO lpcmi)
{
    if (!m_cidl || !m_pDataObj)
        return E_FAIL;

    DoDeleteAsync(m_pDataObj, lpcmi->fMask);
    return S_OK;
}

HRESULT CDefaultContextMenu::DoCopyOrCut(LPCMINVOKECOMMANDINFO lpcmi, BOOL bCopy)
{
    if (!m_cidl || !m_pDataObj)
        return E_FAIL;

    if (!bCopy)
    {
        FORMATETC formatetc;
        STGMEDIUM medium;
        InitFormatEtc(formatetc, RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT), TYMED_HGLOBAL);
        m_pDataObj->GetData(&formatetc, &medium);
        DWORD * pdwFlag = (DWORD*)GlobalLock(medium.hGlobal);
        if (pdwFlag)
            *pdwFlag = DROPEFFECT_MOVE;
        GlobalUnlock(medium.hGlobal);
        m_pDataObj->SetData(&formatetc, &medium, TRUE);
    }

    return OleSetClipboard(m_pDataObj);
}

HRESULT CDefaultContextMenu::DoRename(LPCMINVOKECOMMANDINFO lpcmi)
{
    CComPtr<IShellBrowser> psb;
    HRESULT hr;

    if (!m_site || !m_cidl)
        return E_FAIL;

    /* Get a pointer to the shell browser */
    hr = IUnknown_QueryService(m_site, SID_IShellBrowser, IID_PPV_ARG(IShellBrowser, &psb));
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    CComPtr<IShellView> lpSV;
    hr = psb->QueryActiveShellView(&lpSV);
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    SVSIF selFlags = SVSI_DESELECTOTHERS | SVSI_EDIT | SVSI_ENSUREVISIBLE | SVSI_FOCUSED | SVSI_SELECT;
    lpSV->SelectItem(m_apidl[0], selFlags);
    return S_OK;
}

HRESULT
CDefaultContextMenu::DoProperties(
    LPCMINVOKECOMMANDINFO lpcmi)
{
    HRESULT hr = S_OK;
    const ITEMIDLIST *pidlParent = m_pidlFolder, *pidlChild;

    if (!pidlParent)
    {
        CComPtr<IPersistFolder2> pf;

        /* pidlFolder is optional */
        if (SUCCEEDED(m_psf->QueryInterface(IID_PPV_ARG(IPersistFolder2, &pf))))
        {
            pf->GetCurFolder((_ITEMIDLIST**)&pidlParent);
        }
    }

    if (m_cidl > 0)
        pidlChild = m_apidl[0];
    else
    {
        /* Set pidlChild to last pidl of current folder */
        if (pidlParent == m_pidlFolder)
            pidlParent = (ITEMIDLIST*)ILClone(pidlParent);

        pidlChild = (ITEMIDLIST*)ILClone(ILFindLastID(pidlParent));
        ILRemoveLastID((ITEMIDLIST*)pidlParent);
    }

    if (_ILIsMyComputer(pidlChild))
    {
        if (32 >= (UINT)ShellExecuteW(lpcmi->hwnd, L"open", L"rundll32.exe shell32.dll,Control_RunDLL sysdm.cpl", NULL, NULL, SW_SHOWNORMAL))
            hr = E_FAIL;
    }
    else if (_ILIsDesktop(pidlChild))
    {
        if (32 >= (UINT)ShellExecuteW(lpcmi->hwnd, L"open", L"rundll32.exe shell32.dll,Control_RunDLL desk.cpl", NULL, NULL, SW_SHOWNORMAL))
            hr = E_FAIL;
    }
    else if (_ILIsDrive(pidlChild))
    {
        WCHAR wszBuf[MAX_PATH];
        ILGetDisplayName(pidlChild, wszBuf);
        if (!SH_ShowDriveProperties(wszBuf, pidlParent, &pidlChild))
            hr = E_FAIL;
    }
    else if (_ILIsNetHood(pidlChild))
    {
        // FIXME path!
        if (32 >= (UINT)ShellExecuteW(NULL, L"open", L"explorer.exe",
                                      L"::{7007ACC7-3202-11D1-AAD2-00805FC1270E}",
                                      NULL, SW_SHOWDEFAULT))
            hr = E_FAIL;
    }
    else if (_ILIsBitBucket(pidlChild))
    {
        /* FIXME: detect the drive path of bitbucket if appropiate */
        if (!SH_ShowRecycleBinProperties(L'C'))
            hr = E_FAIL;
    }
    else
    {
        if (m_cidl > 1)
            WARN("SHMultiFileProperties is not yet implemented\n");

        STRRET strFile;
        hr = m_psf->GetDisplayNameOf(pidlChild, SHGDN_FORPARSING, &strFile);
        if (SUCCEEDED(hr))
        {
            WCHAR wszBuf[MAX_PATH];
            hr = StrRetToBufW(&strFile, pidlChild, wszBuf, _countof(wszBuf));
            if (SUCCEEDED(hr))
                hr = SH_ShowPropertiesDialog(wszBuf, pidlParent, &pidlChild);
            else
                ERR("StrRetToBufW failed\n");
        }
        else
            ERR("IShellFolder_GetDisplayNameOf failed for apidl\n");
    }

    /* Free allocated PIDLs */
    if (pidlParent != m_pidlFolder)
        ILFree((ITEMIDLIST*)pidlParent);
    if (m_cidl < 1 || pidlChild != m_apidl[0])
        ILFree((ITEMIDLIST*)pidlChild);

    return hr;
}

HRESULT
CDefaultContextMenu::DoFormat(
    LPCMINVOKECOMMANDINFO lpcmi)
{
    char szDrive[8] = {0};

    if (!_ILGetDrive(m_apidl[0], szDrive, sizeof(szDrive)))
    {
        ERR("pidl is not a drive\n");
        return E_FAIL;
    }

    SHFormatDrive(lpcmi->hwnd, szDrive[0] - 'A', SHFMT_ID_DEFAULT, 0);
    return S_OK;
}

// This code is taken from CNewMenu and should be shared between the 2 classes
HRESULT
CDefaultContextMenu::DoCreateNewFolder(
    LPCMINVOKECOMMANDINFO lpici)
{
    WCHAR wszPath[MAX_PATH];
    WCHAR wszName[MAX_PATH];
    WCHAR wszNewFolder[25];
    HRESULT hr;

    /* Get folder path */
    hr = SHGetPathFromIDListW(m_pidlFolder, wszPath);
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    if (!LoadStringW(shell32_hInstance, IDS_NEWFOLDER, wszNewFolder, _countof(wszNewFolder)))
        return E_FAIL;

    /* Create the name of the new directory */
    if (!PathYetAnotherMakeUniqueName(wszName, wszPath, NULL, wszNewFolder))
        return E_FAIL;

    /* Create the new directory and show the appropriate dialog in case of error */
    if (SHCreateDirectory(lpici->hwnd, wszName) != ERROR_SUCCESS)
        return E_FAIL;

    /* Show and select the new item in the def view */
    LPITEMIDLIST pidl;
    PITEMID_CHILD pidlNewItem;
    CComPtr<IShellView> psv;

    /* Notify the view object about the new item */
    SHChangeNotify(SHCNE_MKDIR, SHCNF_PATHW, (LPCVOID)wszName, NULL);

    if (!m_site)
        return S_OK;

    /* Get a pointer to the shell view */
    hr = IUnknown_QueryService(m_site, SID_IFolderView, IID_PPV_ARG(IShellView, &psv));
    if (FAILED_UNEXPECTEDLY(hr))
        return S_OK;

    /* Attempt to get the pidl of the new item */
    hr = SHILCreateFromPathW(wszName, &pidl, NULL);
    if (FAILED_UNEXPECTEDLY(hr))
        return hr;

    pidlNewItem = ILFindLastID(pidl);

    hr = psv->SelectItem(pidlNewItem, SVSI_DESELECTOTHERS | SVSI_EDIT | SVSI_ENSUREVISIBLE |
                          SVSI_FOCUSED | SVSI_SELECT);

    SHFree(pidl);

    return hr;
}

PDynamicShellEntry CDefaultContextMenu::GetDynamicEntry(UINT idCmd)
{
    PDynamicShellEntry pEntry = m_pDynamicEntries;

    while(pEntry && idCmd > pEntry->iIdCmdFirst + pEntry->NumIds)
        pEntry = pEntry->pNext;

    if (!pEntry)
        return NULL;

    if (idCmd < pEntry->iIdCmdFirst || idCmd > pEntry->iIdCmdFirst + pEntry->NumIds)
        return NULL;

    return pEntry;
}

//FIXME: 260 is correct, but should this be part of the SDK or just MAX_PATH?
#define MAX_VERB 260

BOOL
CDefaultContextMenu::MapVerbToCmdId(PVOID Verb, PUINT idCmd, BOOL IsUnicode)
{
    WCHAR UnicodeStr[MAX_VERB];

    /* Loop through all the static verbs looking for a match */
    for (UINT i = 0; i < _countof(g_StaticInvokeCmdMap); i++)
    {
        /* We can match both ANSI and unicode strings */
        if (IsUnicode)
        {
            /* The static verbs are ANSI, get a unicode version before doing the compare */
            SHAnsiToUnicode(g_StaticInvokeCmdMap[i].szStringVerb, UnicodeStr, MAX_VERB);
            if (!wcscmp(UnicodeStr, (LPWSTR)Verb))
            {
                /* Return the Corresponding Id */
                *idCmd = g_StaticInvokeCmdMap[i].IntVerb;
                return TRUE;
            }
        }
        else
        {
            if (!strcmp(g_StaticInvokeCmdMap[i].szStringVerb, (LPSTR)Verb))
            {
                *idCmd = g_StaticInvokeCmdMap[i].IntVerb;
                return TRUE;
            }
        }
    }

    return FALSE;
}

HRESULT
CDefaultContextMenu::DoDynamicShellExtensions(
    LPCMINVOKECOMMANDINFO lpcmi)
{    
    TRACE("verb %p first %x last %x", lpcmi->lpVerb, m_iIdSHEFirst, m_iIdSHELast);

    UINT idCmd = LOWORD(lpcmi->lpVerb);
    PDynamicShellEntry pEntry = GetDynamicEntry(idCmd);
    if (!pEntry)
        return E_FAIL;

    /* invoke the dynamic context menu */
    lpcmi->lpVerb = MAKEINTRESOURCEA(idCmd - pEntry->iIdCmdFirst);
    return pEntry->pCM->InvokeCommand(lpcmi);
}

DWORD
CDefaultContextMenu::BrowserFlagsFromVerb(LPCMINVOKECOMMANDINFO lpcmi, PStaticShellEntry pEntry)
{
    CComPtr<IShellBrowser> psb;
    HWND hwndTree;
    LPCWSTR FlagsName;
    WCHAR wszKey[256];
    HRESULT hr;
    DWORD wFlags;
    DWORD cbVerb;

    if (!m_site)
        return 0;

    /* Get a pointer to the shell browser */
    hr = IUnknown_QueryService(m_site, SID_IShellBrowser, IID_PPV_ARG(IShellBrowser, &psb));
    if (FAILED_UNEXPECTEDLY(hr))
        return 0;

    /* See if we are in Explore or Browse mode. If the browser's tree is present, we are in Explore mode.*/
    if (SUCCEEDED(psb->GetControlWindow(FCW_TREE, &hwndTree)) && hwndTree)
        FlagsName = L"ExplorerFlags";
    else
        FlagsName = L"BrowserFlags";

    /* Try to get the flag from the verb */
    hr = StringCbPrintfW(wszKey, sizeof(wszKey), L"shell\\%s", pEntry->szVerb);
    if (!SUCCEEDED(hr))
        return 0;

    cbVerb = sizeof(wFlags);
    if (RegGetValueW(pEntry->hkClass, wszKey, FlagsName, RRF_RT_REG_DWORD, NULL, &wFlags, &cbVerb) == ERROR_SUCCESS)
    {
        return wFlags;
    }

    return 0;
}

HRESULT
CDefaultContextMenu::TryToBrowse(
    LPCMINVOKECOMMANDINFO lpcmi, LPCITEMIDLIST pidl, DWORD wFlags)
{
    CComPtr<IShellBrowser> psb;
    HRESULT hr;

    if (!m_site)
        return E_FAIL;

    /* Get a pointer to the shell browser */
    hr = IUnknown_QueryService(m_site, SID_IShellBrowser, IID_PPV_ARG(IShellBrowser, &psb));
    if (FAILED_UNEXPECTEDLY(hr))
        return 0;

    return psb->BrowseObject(ILCombine(m_pidlFolder, pidl), wFlags);
}

HRESULT
CDefaultContextMenu::InvokePidl(LPCMINVOKECOMMANDINFO lpcmi, LPCITEMIDLIST pidl, PStaticShellEntry pEntry)
{
    LPITEMIDLIST pidlFull = ILCombine(m_pidlFolder, pidl);
    if (pidlFull == NULL)
    {
        return E_FAIL;
    }

    WCHAR wszPath[MAX_PATH];
    BOOL bHasPath = SHGetPathFromIDListW(pidlFull, wszPath);

    WCHAR wszDir[MAX_PATH];
    if (bHasPath)
    {
        wcscpy(wszDir, wszPath);
        PathRemoveFileSpec(wszDir);
    }
    else
    {
        SHGetPathFromIDListW(m_pidlFolder, wszDir);
    }

    SHELLEXECUTEINFOW sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.hwnd = lpcmi->hwnd;
    sei.nShow = SW_SHOWNORMAL;
    sei.lpVerb = pEntry->szVerb;
    sei.lpDirectory = wszDir;
    sei.lpIDList = pidlFull;
    sei.hkeyClass = pEntry->hkClass;
    sei.fMask = SEE_MASK_CLASSKEY | SEE_MASK_IDLIST;
    if (bHasPath)
    {
        sei.lpFile = wszPath;
    }

    ShellExecuteExW(&sei);

    ILFree(pidlFull);

    return S_OK;
}

HRESULT
CDefaultContextMenu::DoStaticShellExtensions(
    LPCMINVOKECOMMANDINFO lpcmi)
{
    PStaticShellEntry pEntry = m_pStaticEntries;
    INT iCmd = LOWORD(lpcmi->lpVerb) - m_iIdSCMFirst;
    HRESULT hr;
    UINT i;

    while (pEntry && (iCmd--) > 0)
        pEntry = pEntry->pNext;

    if (iCmd > 0)
        return E_FAIL;

    /* Get the browse flags to see if we need to browse */
    DWORD wFlags = BrowserFlagsFromVerb(lpcmi, pEntry);
    BOOL bBrowsed = FALSE;

    for (i=0; i < m_cidl; i++)
    {
        /* Check if we need to browse */
        if (wFlags > 0)
        {
            /* In xp if we have browsed, we don't open any more folders .
             * In win7 we browse to the first folder we find and
             * open new windows fo for each of the rest of the folders */
            if (bBrowsed)
                continue;

            hr = TryToBrowse(lpcmi, m_apidl[i], wFlags);
            if (SUCCEEDED(hr))
            {
                bBrowsed = TRUE;
                continue;
            }
        }

        InvokePidl(lpcmi, m_apidl[i], pEntry);
    }

    return S_OK;
}

HRESULT
WINAPI
CDefaultContextMenu::InvokeCommand(
    LPCMINVOKECOMMANDINFO lpcmi)
{
    CMINVOKECOMMANDINFO LocalInvokeInfo;
    HRESULT Result;
    UINT CmdId;

    /* Take a local copy of the fixed members of the
       struct as we might need to modify the verb */
    LocalInvokeInfo = *lpcmi;

    /* Check if this is a string verb */
    if (HIWORD(LocalInvokeInfo.lpVerb))
    {
        /* Get the ID which corresponds to this verb, and update our local copy */
        if (MapVerbToCmdId((LPVOID)LocalInvokeInfo.lpVerb, &CmdId, FALSE))
            LocalInvokeInfo.lpVerb = MAKEINTRESOURCEA(CmdId);
    }

    /* Check if this is a Id */
    switch (LOWORD(LocalInvokeInfo.lpVerb))
    {
    case FCIDM_SHVIEW_BIGICON:
    case FCIDM_SHVIEW_SMALLICON:
    case FCIDM_SHVIEW_LISTVIEW:
    case FCIDM_SHVIEW_REPORTVIEW:
    case 0x30: /* FIX IDS in resource files */
    case 0x31:
    case 0x32:
    case 0x33:
    case FCIDM_SHVIEW_AUTOARRANGE:
    case FCIDM_SHVIEW_SNAPTOGRID:
        Result = NotifyShellViewWindow(&LocalInvokeInfo, FALSE);
        break;
    case FCIDM_SHVIEW_REFRESH:
        Result = DoRefresh(&LocalInvokeInfo);
        break;
    case FCIDM_SHVIEW_INSERT:
        Result = DoPaste(&LocalInvokeInfo, FALSE);
        break;
    case FCIDM_SHVIEW_INSERTLINK:
        Result = DoPaste(&LocalInvokeInfo, TRUE);
        break;
    case FCIDM_SHVIEW_OPEN:
    case FCIDM_SHVIEW_EXPLORE:
        Result = DoOpenOrExplore(&LocalInvokeInfo);
        break;
    case FCIDM_SHVIEW_COPY:
    case FCIDM_SHVIEW_CUT:
        Result = DoCopyOrCut(&LocalInvokeInfo, LOWORD(LocalInvokeInfo.lpVerb) == FCIDM_SHVIEW_COPY);
        break;
    case FCIDM_SHVIEW_CREATELINK:
        Result = DoCreateLink(&LocalInvokeInfo);
        break;
    case FCIDM_SHVIEW_DELETE:
        Result = DoDelete(&LocalInvokeInfo);
        break;
    case FCIDM_SHVIEW_RENAME:
        Result = DoRename(&LocalInvokeInfo);
        break;
    case FCIDM_SHVIEW_PROPERTIES:
        Result = DoProperties(&LocalInvokeInfo);
        break;
    case 0x7ABC:
        Result = DoFormat(&LocalInvokeInfo);
        break;
    case FCIDM_SHVIEW_NEWFOLDER:
        Result = DoCreateNewFolder(&LocalInvokeInfo);
        break;
    default:
        Result = E_UNEXPECTED;
        break;
    }

    /* Check for ID's we didn't find a handler for */
    if (Result == E_UNEXPECTED)
    {
        if (m_iIdSHEFirst && m_iIdSHELast)
        {
            if (LOWORD(LocalInvokeInfo.lpVerb) >= m_iIdSHEFirst && LOWORD(LocalInvokeInfo.lpVerb) <= m_iIdSHELast)
                Result = DoDynamicShellExtensions(&LocalInvokeInfo);
        }

        if (m_iIdSCMFirst && m_iIdSCMLast)
        {
            if (LOWORD(LocalInvokeInfo.lpVerb) >= m_iIdSCMFirst && LOWORD(LocalInvokeInfo.lpVerb) <= m_iIdSCMLast)
                Result = DoStaticShellExtensions(&LocalInvokeInfo);
        }
    }

    if (Result == E_UNEXPECTED)
        FIXME("Unhandled Verb %xl\n", LOWORD(LocalInvokeInfo.lpVerb));

    return Result;
}

HRESULT
WINAPI
CDefaultContextMenu::GetCommandString(
    UINT_PTR idCommand,
    UINT uFlags,
    UINT* lpReserved,
    LPSTR lpszName,
    UINT uMaxNameLen)
{
    /* We don't handle the help text yet */
    if (uFlags == GCS_HELPTEXTA ||
        uFlags == GCS_HELPTEXTW)
    {
        return E_NOTIMPL;
    }

    /* Loop looking for a matching Id */
    for (UINT i = 0; i < _countof(g_StaticInvokeCmdMap); i++)
    {
        if (g_StaticInvokeCmdMap[i].IntVerb == idCommand)
        {
            /* Validation just returns S_OK on a match */
            if (uFlags == GCS_VALIDATEA || uFlags == GCS_VALIDATEW)
                return S_OK;

            /* Return a copy of the ANSI verb */
            if (uFlags == GCS_VERBA)
                return StringCchCopyA(lpszName, uMaxNameLen, g_StaticInvokeCmdMap[i].szStringVerb);

            /* Convert the ANSI verb to unicode and return that */
            if (uFlags == GCS_VERBW)
            {
                if (SHAnsiToUnicode(g_StaticInvokeCmdMap[i].szStringVerb, (LPWSTR)lpszName, uMaxNameLen))
                    return S_OK;
            }
        }
    }

    return E_INVALIDARG;
}

HRESULT
WINAPI
CDefaultContextMenu::HandleMenuMsg(
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    /* FIXME: Should we implement this as well? */
    return S_OK;
}

HRESULT 
WINAPI 
CDefaultContextMenu::HandleMenuMsg2(
    UINT uMsg, 
    WPARAM wParam, 
    LPARAM lParam, 
    LRESULT *plResult)
{
    switch (uMsg)
    {
    case WM_INITMENUPOPUP:
    {
        PDynamicShellEntry pEntry = m_pDynamicEntries;
        while (pEntry)
        {
            SHForwardContextMenuMsg(pEntry->pCM, uMsg, wParam, lParam, plResult, TRUE);
            pEntry = pEntry->pNext;
        }
        break;
    }
    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT* pDrawStruct = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        PDynamicShellEntry pEntry = GetDynamicEntry(pDrawStruct->itemID);
        if (pEntry)
            SHForwardContextMenuMsg(pEntry->pCM, uMsg, wParam, lParam, plResult, TRUE);
        break;
    }
    case WM_MEASUREITEM:
    {
        MEASUREITEMSTRUCT* pMeasureStruct = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        PDynamicShellEntry pEntry = GetDynamicEntry(pMeasureStruct->itemID);
        if (pEntry)
            SHForwardContextMenuMsg(pEntry->pCM, uMsg, wParam, lParam, plResult, TRUE);
        break;
    }
    case WM_MENUCHAR :
        /* FIXME */
        break;
    default:
        ERR("Got unknown message:%d\n", uMsg);
    }
   return S_OK;
}

HRESULT
WINAPI
CDefaultContextMenu::SetSite(IUnknown *pUnkSite)
{
    m_site = pUnkSite;
    return S_OK;
}

HRESULT 
WINAPI 
CDefaultContextMenu::GetSite(REFIID riid, void **ppvSite)
{
    if (!m_site)
        return E_FAIL;

    return m_site->QueryInterface(riid, ppvSite);
}

static
HRESULT
CDefaultContextMenu_CreateInstance(const DEFCONTEXTMENU *pdcm, REFIID riid, void **ppv)
{
    return ShellObjectCreatorInit<CDefaultContextMenu>(pdcm, riid, ppv);
}

/*************************************************************************
 * SHCreateDefaultContextMenu            [SHELL32.325] Vista API
 *
 */

static void AddClassKey(const WCHAR * szClass, HKEY* buffer, UINT* cKeys)
{
    LSTATUS result;
    HKEY hkey;
    result = RegOpenKeyExW(HKEY_CLASSES_ROOT, szClass, 0, KEY_READ | KEY_QUERY_VALUE, &hkey);
    if (result != ERROR_SUCCESS)
        return;

    buffer[*cKeys] = hkey;
    *cKeys +=1;
}

void HackFillKeys(DEFCONTEXTMENU *pdcm, HKEY* buffer)
{
    PCUITEMID_CHILD pidl = pdcm->apidl[0];
    pdcm->cKeys = 0;
    pdcm->aKeys = buffer;

    if (_ILIsValue(pidl))
    {
        FileStructW* pFileData = _ILGetFileStructW(pidl);
        LPWSTR extension = PathFindExtension(pFileData->wszName);

        if (extension)
        {
            AddClassKey(extension, buffer, &pdcm->cKeys);

            WCHAR wszClass[40], wszClass2[40];
            DWORD dwSize = sizeof(wszClass);
            if (RegGetValueW(HKEY_CLASSES_ROOT, extension, NULL, RRF_RT_REG_SZ, NULL, wszClass, &dwSize) == ERROR_SUCCESS)
            {
                swprintf(wszClass2, L"%s//%s", extension, wszClass);

                AddClassKey(wszClass, buffer, &pdcm->cKeys);
                AddClassKey(wszClass2, buffer, &pdcm->cKeys);
            }

            swprintf(wszClass2, L"SystemFileAssociations//%s", extension);
            AddClassKey(wszClass2, buffer, &pdcm->cKeys);

            if (RegGetValueW(HKEY_CLASSES_ROOT, extension, L"PerceivedType ", RRF_RT_REG_SZ, NULL, wszClass, &dwSize) == ERROR_SUCCESS)
            {
                swprintf(wszClass2, L"SystemFileAssociations//%s", wszClass);
                AddClassKey(wszClass2, buffer, &pdcm->cKeys);
            }
        }

        AddClassKey(L"AllFilesystemObjects", buffer, &pdcm->cKeys);
        AddClassKey(L"*", buffer, &pdcm->cKeys);
    }
    else if (_ILIsSpecialFolder(pidl))
    {
        GUID *pGuid = _ILGetGUIDPointer(pidl);
        if (pGuid)
        {
            LPOLESTR pwszCLSID;
            WCHAR key[60];

            wcscpy(key, L"CLSID\\");
            HRESULT hr = StringFromCLSID(*pGuid, &pwszCLSID);
            if (hr == S_OK)
            {
                wcscpy(&key[6], pwszCLSID);
                AddClassKey(key, buffer, &pdcm->cKeys);
            }
        }
        AddClassKey(L"Folder", buffer, &pdcm->cKeys);
    }
    else if (_ILIsFolder(pidl))
    {
        AddClassKey(L"AllFilesystemObjects", buffer, &pdcm->cKeys);
        AddClassKey(L"Directory", buffer, &pdcm->cKeys);
        AddClassKey(L"Folder", buffer, &pdcm->cKeys);
    }
    else if (_ILIsDrive(pidl))
    {
        AddClassKey(L"Drive", buffer, &pdcm->cKeys);
        AddClassKey(L"Folder", buffer, &pdcm->cKeys);
    }
}

HRESULT
WINAPI
SHCreateDefaultContextMenu(const DEFCONTEXTMENU *pdcm, REFIID riid, void **ppv)
{
   /* HACK: move to the shell folders implementation */
    HKEY hkeyHack[16];
    if (!pdcm->aKeys && pdcm->cidl)
        HackFillKeys((DEFCONTEXTMENU *)pdcm, hkeyHack);

    return CDefaultContextMenu_CreateInstance(pdcm, riid, ppv);
}

/*************************************************************************
 * CDefFolderMenu_Create2            [SHELL32.701]
 *
 */

HRESULT
WINAPI
CDefFolderMenu_Create2(
    PCIDLIST_ABSOLUTE pidlFolder,
    HWND hwnd,
    UINT cidl,
    PCUITEMID_CHILD_ARRAY apidl,
    IShellFolder *psf,
    LPFNDFMCALLBACK lpfn,
    UINT nKeys,
    const HKEY *ahkeyClsKeys,
    IContextMenu **ppcm)
{
    DEFCONTEXTMENU pdcm;
    pdcm.hwnd = hwnd;
    pdcm.pcmcb = NULL;
    pdcm.pidlFolder = pidlFolder;
    pdcm.psf = psf;
    pdcm.cidl = cidl;
    pdcm.apidl = apidl;
    pdcm.punkAssociationInfo = NULL;
    pdcm.cKeys = nKeys;
    pdcm.aKeys = ahkeyClsKeys;

    HRESULT hr = SHCreateDefaultContextMenu(&pdcm, IID_PPV_ARG(IContextMenu, ppcm));
    return hr;
}

