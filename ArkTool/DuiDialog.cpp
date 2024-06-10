#include "DuiDialog.h"

void DuiDialog::Notify(TNotifyUI& msg)
{
	if (msg.sType == DUI_MSGTYPE_WINDOWINIT)
	{
		m_nameEdit = static_cast<CEditUI*>(m_PaintManager.FindControl(_T("nameEdit")));
		if (m_nameEdit)
		{
			m_nameEdit->SetText(m_filename.c_str());
		}

	}else if (msg.sType == DUI_MSGTYPE_CLICK)
	{
		if (msg.pSender->GetName() == _T("cancelBtn")) 
		{ 
			Close(); 
			return;
		}
		else if (msg.pSender->GetName() == _T("saveBtn")) 
		{ 
			m_filename = m_nameEdit->GetText();
			Close(); 
			return; 
		}
	}
}
