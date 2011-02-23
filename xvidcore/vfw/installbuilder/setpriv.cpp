#include <windows.h>
#include <aclapi.h>
#include <stdio.h>

BOOL SetPrivilege(
    HANDLE hToken,          // access token handle
    LPCTSTR lpszPrivilege,  // name of privilege to enable/disable
    BOOL bEnablePrivilege   // to enable or disable privilege
    ) 
{
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if ( !LookupPrivilegeValue( 
            NULL,            // lookup privilege on local system
            lpszPrivilege,   // privilege to lookup 
            &luid ) )        // receives LUID of privilege
    {
        OutputDebugString(L"LookupPrivilegeValue error"); 
        return FALSE; 
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    if (bEnablePrivilege)
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else
        tp.Privileges[0].Attributes = 0;

    // Enable the privilege or disable all privileges.

    if ( !AdjustTokenPrivileges(
           hToken, 
           FALSE, 
           &tp, 
           sizeof(TOKEN_PRIVILEGES), 
           (PTOKEN_PRIVILEGES) NULL, 
           (PDWORD) NULL) )
    { 
          OutputDebugString(L"AdjustTokenPrivileges error"); 
          return FALSE; 
    } 

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)

    {
          OutputDebugString(L"The token does not have the specified privilege.");
          return FALSE;
    } 

    return TRUE;
}

BOOL TakeOwnership(LPTSTR lpszRegKey) 
{
    BOOL bRetval = FALSE;

    HANDLE hToken = NULL; 
	PSECURITY_DESCRIPTOR pSecurityDescriptor = NULL;
    PSID pSIDAdmin = NULL;
    PACL pOldACL = NULL;
    PACL pNewACL = NULL;
    SID_IDENTIFIER_AUTHORITY SIDAuthNT = SECURITY_NT_AUTHORITY;
    EXPLICIT_ACCESS ea;
    DWORD dwRes;

    // Create a SID for the BUILTIN\Administrators group.
    if (!AllocateAndInitializeSid(&SIDAuthNT, 2,
                     SECURITY_BUILTIN_DOMAIN_RID,
                     DOMAIN_ALIAS_RID_ADMINS,
                     0, 0, 0, 0, 0, 0,
                     &pSIDAdmin)) 
    {
        OutputDebugString(L"AllocateAndInitializeSid (Admin) error");
        goto Cleanup;
    }

	// Get DACL of current object.
	dwRes = GetNamedSecurityInfo(
		lpszRegKey,
		SE_REGISTRY_KEY,
		DACL_SECURITY_INFORMATION,
		NULL,
		NULL,
		&pOldACL,
		NULL,
		&pSecurityDescriptor);

    if (ERROR_SUCCESS != dwRes) 
    {
        bRetval = FALSE;
        OutputDebugString(L"GetNamedSecurityInfo failed.");
        goto Cleanup;
    }

    ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));

    // Set full control for Administrators.
    ea.grfAccessPermissions = GENERIC_ALL;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = NO_INHERITANCE;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    ea.Trustee.ptstrName = (LPTSTR) pSIDAdmin;

    if (ERROR_SUCCESS != SetEntriesInAcl(1,&ea,pOldACL,&pNewACL))
    {
        OutputDebugString(L"Failed SetEntriesInAcl\n");
        goto Cleanup;
    }

    // Try to modify the object's DACL.
    dwRes = SetNamedSecurityInfo(
        lpszRegKey,                  // name of the object
        SE_REGISTRY_KEY,             // type of object
        DACL_SECURITY_INFORMATION,   // change only the object's DACL
        NULL, NULL,                  // do not change owner or group
        pNewACL,                     // DACL specified
        NULL);                       // do not change SACL

    if (ERROR_SUCCESS == dwRes) 
    {
        OutputDebugString(L"Successfully changed DACL\n");
        bRetval = TRUE;
        // No more processing needed.
        goto Cleanup;
    }
    if (dwRes != ERROR_ACCESS_DENIED)
    {
        OutputDebugString(L"First SetNamedSecurityInfo call failed"); 
        goto Cleanup;
    }

    // If the preceding call failed because access was denied, 
    // enable the SE_TAKE_OWNERSHIP_NAME privilege, create a SID for 
    // the Administrators group, take ownership of the object, and 
    // disable the privilege. Then try again to set the object's DACL.

    // Open a handle to the access token for the calling process.
    if (!OpenProcessToken(GetCurrentProcess(), 
                          TOKEN_ADJUST_PRIVILEGES, 
                          &hToken)) 
       {
          OutputDebugString(L"OpenProcessToken failed"); 
          goto Cleanup; 
       } 

    // Enable the SE_TAKE_OWNERSHIP_NAME privilege.
    if (!SetPrivilege(hToken, SE_TAKE_OWNERSHIP_NAME, TRUE)) 
    {
        OutputDebugString(L"You must be logged on as Administrator.\n");
        goto Cleanup; 
    }

    // Set the owner in the object's security descriptor.
    dwRes = SetNamedSecurityInfo(
        lpszRegKey,                  // name of the object
        SE_REGISTRY_KEY,             // type of object
        OWNER_SECURITY_INFORMATION,  // change only the object's owner
        pSIDAdmin,                   // SID of Administrator group
        NULL,
        NULL,
        NULL); 

    if (dwRes != ERROR_SUCCESS) 
    {
        OutputDebugString(L"Could not set owner. Error: %u\n"); 
        goto Cleanup;
    }
        
    // Disable the SE_TAKE_OWNERSHIP_NAME privilege.
    if (!SetPrivilege(hToken, SE_TAKE_OWNERSHIP_NAME, FALSE)) 
    {
        OutputDebugString(L"Failed SetPrivilege call unexpectedly.\n");
        goto Cleanup;
    }

    // Try again to modify the object's DACL,
    // now that we are the owner.
    dwRes = SetNamedSecurityInfo(
        lpszRegKey,                  // name of the object
        SE_REGISTRY_KEY,             // type of object
        DACL_SECURITY_INFORMATION,   // change only the object's DACL
        NULL, NULL,                  // do not change owner or group
        pNewACL,                     // DACL specified
        NULL);                       // do not change SACL

    if (dwRes == ERROR_SUCCESS)
    {
        OutputDebugString(L"Successfully changed DACL\n");
        bRetval = TRUE; 
    }
    else
    {
        OutputDebugString(L"Second SetNamedSecurityInfo call failed"); 
    }

Cleanup:

	if (pSecurityDescriptor)
		LocalFree(pSecurityDescriptor);

    if (pSIDAdmin)
        FreeSid(pSIDAdmin); 

    if (pNewACL)
       LocalFree(pNewACL);

    if (hToken)
       CloseHandle(hToken);

    return bRetval;

}

int CALLBACK WinMain(
  __in  HINSTANCE hInstance,
  __in  HINSTANCE hPrevInstance,
  __in  LPSTR lpCmdLine,
  __in  int nCmdShow
)
{
	BOOL bRetval;

	bRetval = TakeOwnership(L"MACHINE\\SOFTWARE\\Classes\\MediaFoundation\\MapVideo4cc"); // target reg-key
	
	bRetval = TakeOwnership(L"MACHINE\\SOFTWARE\\Microsoft\\DirectShow\\Preferred"); // target reg-key

	return bRetval;
}
