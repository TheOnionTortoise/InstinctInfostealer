#include <windows.h>
#include <uiautomation.h>
#include <atlbase.h>
#include <string>
#include <iostream>
#include <vector>
#include <map>
#include <comutil.h>
#include <thread>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <io.h>
#include <fcntl.h>

// --- Main Class for Monitoring ---
class UIMonitor {
private:
    CComPtr<IUIAutomation> m_automation;
    CComPtr<IUIAutomationCondition> m_searchCondition;
    CComPtr<IUIAutomationElement> m_docElement;

    // Map to track values and only report changes
    std::map<std::wstring, std::wstring> m_trackedValues;
    HWND m_lastScannedHwnd = NULL;
    std::wstring m_lastDocumentTitle;
    CComPtr<IUIAutomationElement> m_lastFocusedElement;
    bool m_wasLastElementPassword = false;

    // --- FINAL: Members for smart password field handling ---
    bool m_isCurrentlyInPasswordField = false;
    std::chrono::steady_clock::time_point m_lastPasswordActivityTime;
    bool m_hasAlreadyFiredThisPause = false;
    std::wstring m_currentPasswordValue; // Track the password value to check if it's empty

public:
    UIMonitor() = default;
    ~UIMonitor() = default;

    bool Initialize() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hr)) {
            std::wcerr << L"Failed to initialize COM." << std::endl;
            return false;
        }

        hr = CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
            __uuidof(IUIAutomation), reinterpret_cast<void**>(&m_automation));
        if (FAILED(hr) || !m_automation) {
            std::wcerr << L"Failed to create UI Automation instance." << std::endl;
            return false;
        }

        VARIANT varEdit, varDoc;
        varEdit.vt = VT_I4; varEdit.lVal = UIA_EditControlTypeId;
        varDoc.vt = VT_I4; varDoc.lVal = UIA_DocumentControlTypeId;
        CComPtr<IUIAutomationCondition> pEditCond, pDocCond;
        m_automation->CreatePropertyCondition(UIA_ControlTypePropertyId, varEdit, &pEditCond);
        m_automation->CreatePropertyCondition(UIA_ControlTypePropertyId, varDoc, &pDocCond);
        m_automation->CreateOrCondition(pEditCond, pDocCond, &m_searchCondition);

        if (!m_searchCondition) {
            std::wcerr << L"Failed to create search condition." << std::endl;
            return false;
        }

        return true;
    }

    void StartMonitoring() {
        std::wcout << L"[START] Universal Web Monitor is running. Press Ctrl+C to exit." << std::endl;

        while (true) {
            ScanActiveBrowserWindow();
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

private:
    void ScanActiveBrowserWindow() {
        HWND hFgWnd = GetForegroundWindow();
        if (!hFgWnd) return;

        wchar_t className[256];
        GetClassNameW(hFgWnd, className, sizeof(className) / sizeof(wchar_t));
        const std::wstring currentClassName = className;
        
        if (currentClassName != L"Chrome_WidgetWin_1" && currentClassName != L"MozillaWindowClass") {
            if (m_lastScannedHwnd != NULL) {
                m_trackedValues.clear();
                m_lastDocumentTitle.clear();
                m_lastFocusedElement.Release();
                m_docElement.Release();
                m_wasLastElementPassword = false;
                m_isCurrentlyInPasswordField = false;
                m_lastScannedHwnd = NULL;
                std::wcout << L"\n[INFO] Switched away from browser. Pausing scan." << std::endl;
            }
            return;
        }

        CComPtr<IUIAutomationElement> focusedElement;
        m_automation->GetFocusedElement(&focusedElement);

        BOOL isSame = FALSE;
        if (m_lastFocusedElement && focusedElement) {
            m_automation->CompareElements(m_lastFocusedElement, focusedElement, &isSame);
        }
        
        // --- FINALIZED LOGIC: Handle both focus change and pausing ---
        if (!isSame) {
            // Trigger 1: User just left a password field.
            if (m_wasLastElementPassword && !m_hasAlreadyFiredThisPause && !m_currentPasswordValue.empty()) {
                std::wcout << L"    [FOCUS] Left password field." << std::endl;
                ClickShowPasswordNear(m_lastFocusedElement);
            }
            m_isCurrentlyInPasswordField = false; // Reset state on focus change
        }

        m_lastFocusedElement = focusedElement;
        if (m_lastFocusedElement) {
            BOOL isPassword = FALSE;
            m_lastFocusedElement->get_CurrentIsPassword(&isPassword);
            m_wasLastElementPassword = isPassword;
            if (isPassword && !m_isCurrentlyInPasswordField) {
                // We have just entered a password field
                m_isCurrentlyInPasswordField = true;
                m_hasAlreadyFiredThisPause = false;
                m_lastPasswordActivityTime = std::chrono::steady_clock::now();
            }
        } else {
            m_wasLastElementPassword = false;
            m_isCurrentlyInPasswordField = false;
        }

        // Trigger 2: User has paused typing in a non-empty password field.
        if (m_isCurrentlyInPasswordField && !m_hasAlreadyFiredThisPause && !m_currentPasswordValue.empty()) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastPasswordActivityTime);
            if (elapsed.count() > 1200) {
                 std::wcout << L"    [PAUSE] User stopped typing in password field." << std::endl;
                 ClickShowPasswordNear(m_lastFocusedElement);
                 m_hasAlreadyFiredThisPause = true;
            }
        }


        CComPtr<IUIAutomationElement> rootElement;
        if (SUCCEEDED(m_automation->ElementFromHandle(hFgWnd, &rootElement)) && rootElement) {
            CComPtr<IUIAutomationCondition> docCondition;
            VARIANT varDoc;
            varDoc.vt = VT_I4; varDoc.lVal = UIA_DocumentControlTypeId;
            m_automation->CreatePropertyCondition(UIA_ControlTypePropertyId, varDoc, &docCondition);
            
            m_docElement.Release();
            if (SUCCEEDED(rootElement->FindFirst(TreeScope_Subtree, docCondition, &m_docElement)) && m_docElement) {
                BSTR bstrName = nullptr;
                m_docElement->get_CurrentName(&bstrName);
                std::wstring currentDocTitle = bstrName ? bstrName : L"";
                SysFreeString(bstrName);

                if (hFgWnd != m_lastScannedHwnd || currentDocTitle != m_lastDocumentTitle) {
                    m_trackedValues.clear();
                    m_lastScannedHwnd = hFgWnd;
                    m_lastDocumentTitle = currentDocTitle;
                    wchar_t windowTitle[256];
                    GetWindowTextW(hFgWnd, windowTitle, 256);
                    std::wcout << L"\n[PAGE] Switched to: " << (currentDocTitle.empty() ? windowTitle : currentDocTitle) << std::endl;
                }
            }
            
            CComPtr<IUIAutomationElementArray> foundElements;
            if (m_docElement && SUCCEEDED(m_docElement->FindAll(TreeScope_Subtree, m_searchCondition, &foundElements))) {
                int length = 0;
                foundElements->get_Length(&length);
                for (int i = 0; i < length; i++) {
                    CComPtr<IUIAutomationElement> element;
                    if (SUCCEEDED(foundElements->GetElement(i, &element)) && element) {
                        ProcessElement(element);
                    }
                }
            }
        }
    }

    void ClickShowPasswordNear(IUIAutomationElement* passwordElement) {
        if (!passwordElement) return;

        CComPtr<IUIAutomationTreeWalker> walker;
        m_automation->get_ControlViewWalker(&walker);
        CComPtr<IUIAutomationElement> parent;
        if (FAILED(walker->GetParentElement(passwordElement, &parent)) || !parent) return;

        CComPtr<IUIAutomationCondition> pButtonCond, pNameCond, pFinalCond;
        VARIANT varButton, varName;
        varButton.vt = VT_I4; varButton.lVal = UIA_ButtonControlTypeId;
        m_automation->CreatePropertyCondition(UIA_ControlTypePropertyId, varButton, &pButtonCond);
        varName.vt = VT_BSTR; varName.bstrVal = SysAllocString(L"Show");
        m_automation->CreatePropertyCondition(UIA_NamePropertyId, varName, &pNameCond);
        SysFreeString(varName.bstrVal);
        m_automation->CreateAndCondition(pButtonCond, pNameCond, &pFinalCond);
        
        CComPtr<IUIAutomationElement> showButton;
        if (SUCCEEDED(parent->FindFirst(TreeScope_Descendants, pFinalCond, &showButton)) && showButton) {
            CComPtr<IUIAutomationInvokePattern> invokePattern;
            if (SUCCEEDED(showButton->GetCurrentPattern(UIA_InvokePatternId, reinterpret_cast<IUnknown**>(&invokePattern))) && invokePattern) {
                std::wcout << L"        [ACTION] Found standard 'Show' button. Clicking it." << std::endl;
                invokePattern->Invoke();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                ProcessElement(passwordElement);
                return;
            }
        }

        std::wcout << L"        [INFO] Standard 'Show' button not found. Assuming an icon exists and clicking its position..." << std::endl;
        RECT passwordRect;
        if (SUCCEEDED(passwordElement->get_CurrentBoundingRectangle(&passwordRect))) {
            long width = passwordRect.right - passwordRect.left;
            long height = passwordRect.bottom - passwordRect.top;
            POINT clickPoint;
            clickPoint.x = passwordRect.left + (width * 0.95);
            clickPoint.y = passwordRect.top + (height / 2);

            SetCursorPos(clickPoint.x, clickPoint.y);
            mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            ProcessElement(passwordElement);

            std::wcout << L"        [ACTION] Clicking icon position again to hide password." << std::endl;
            SetCursorPos(clickPoint.x, clickPoint.y);
            mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        } else {
            std::wcout << L"        [ERROR] Could not get bounding rectangle of password field to calculate click position." << std::endl;
        }
    }

    void ProcessElement(IUIAutomationElement* element) {
        std::wstring runtimeId = GetRuntimeIdString(element);
        if (runtimeId.empty()) return;

        CComPtr<IUIAutomationValuePattern> valuePattern;
        if (SUCCEEDED(element->GetCurrentPattern(UIA_ValuePatternId, reinterpret_cast<IUnknown**>(&valuePattern))) && valuePattern) {
            BSTR bstrVal = nullptr;
            valuePattern->get_CurrentValue(&bstrVal);
            std::wstring currentValue = bstrVal ? bstrVal : L"";
            SysFreeString(bstrVal);

            if (m_trackedValues.find(runtimeId) == m_trackedValues.end() || m_trackedValues[runtimeId] != currentValue) {
                m_trackedValues[runtimeId] = currentValue;

                BSTR bstrName = nullptr;
                element->get_CurrentName(&bstrName);
                std::wstring nameStr = bstrName ? bstrName : L"(no name)";
                SysFreeString(bstrName);

                BOOL isPassword = FALSE;
                element->get_CurrentIsPassword(&isPassword);

                if (isPassword) {
                    m_currentPasswordValue = currentValue; // Keep track of the current value
                    m_lastPasswordActivityTime = std::chrono::steady_clock::now(); // Reset timer on every keystroke
                    m_hasAlreadyFiredThisPause = false; // Allow the pause trigger to fire again
                }

                std::wcout << L"    [INPUT] '" << nameStr
                    << L"' -> " << (isPassword ? L"********" : L"\"" + currentValue + L"\"") << std::endl;
            }
        }
    }

    std::wstring GetRuntimeIdString(IUIAutomationElement* element) {
        SAFEARRAY* runtimeIdArray = nullptr;
        element->GetRuntimeId(&runtimeIdArray);
        if (!runtimeIdArray) return L"";

        std::wstringstream ss;
        LONG lBound{}, uBound{};
        SafeArrayGetLBound(runtimeIdArray, 1, &lBound);
        SafeArrayGetUBound(runtimeIdArray, 1, &uBound);

        for (LONG i = lBound; i <= uBound; ++i) {
            int val{};
            SafeArrayGetElement(runtimeIdArray, &i, &val);
            ss << val << L'.';
        }
        SafeArrayDestroy(runtimeIdArray);
        return ss.str();
    }
};

int wmain() {
    _setmode(_fileno(stdout), _O_U16TEXT);

    UIMonitor monitor;
    if (!monitor.Initialize()) {
        return 1;
    }

    monitor.StartMonitoring();

    CoUninitialize();
    return 0;
}
