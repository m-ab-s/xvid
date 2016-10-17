Function Get-IniContent {  
    <#  
    .Synopsis  
        Gets the content of an INI file  
          
    .Description  
        Gets the content of an INI file and returns it as a hashtable  
          
    .Notes  
        Author        : Oliver Lipkau <oliver@lipkau.net>  
        Blog          : http://oliver.lipkau.net/blog/  
        Source        : https://github.com/lipkau/PsIni 
                        http://gallery.technet.microsoft.com/scriptcenter/ea40c1ef-c856-434b-b8fb-ebd7a76e8d91 
        Version       : 1.0 - 2010/03/12 - Initial release  
                        1.1 - 2014/12/11 - Typo (Thx SLDR) 
                                           Typo (Thx Dave Stiff) 
          
        #Requires -Version 2.0  
          
    .Inputs  
        System.String  
          
    .Outputs  
        System.Collections.Hashtable  
          
    .Parameter FilePath  
        Specifies the path to the input file.  
          
    .Example  
        $FileContent = Get-IniContent "C:\myinifile.ini"  
        -----------  
        Description  
        Saves the content of the c:\myinifile.ini in a hashtable called $FileContent  
      
    .Example  
        $inifilepath | $FileContent = Get-IniContent  
        -----------  
        Description  
        Gets the content of the ini file passed through the pipe into a hashtable called $FileContent  
      
    .Example  
        C:\PS>$FileContent = Get-IniContent "c:\settings.ini"  
        C:\PS>$FileContent["Section"]["Key"]  
        -----------  
        Description  
        Returns the key "Key" of the section "Section" from the C:\settings.ini file  
          
    .Link  
        Out-IniFile  
    #>  
      
    [CmdletBinding()]  
    Param(  
        [ValidateNotNullOrEmpty()]  
        [ValidateScript({(Test-Path $_) -and ((Get-Item $_).Extension -eq ".ini")})]  
        [Parameter(ValueFromPipeline=$True,Mandatory=$True)]  
        [string]$FilePath  
    )  
      
    Begin  
        {Write-Verbose "$($MyInvocation.MyCommand.Name):: Function started"}  
          
    Process  
    {  
        Write-Verbose "$($MyInvocation.MyCommand.Name):: Processing file: $Filepath"  
              
        $ini = @{}  
        switch -regex -file $FilePath  
        {  
            "^\[(.+)\]$" # Section  
            {  
                $section = $matches[1]  
                $ini[$section] = @{}  
                $CommentCount = 0  
            }  
            "^(;.*)$" # Comment  
            {  
                if (!($section))  
                {  
                    $section = "No-Section"  
                    $ini[$section] = @{}  
                }  
                $value = $matches[1]  
                $CommentCount = $CommentCount + 1  
                $name = "Comment" + $CommentCount  
                $ini[$section][$name] = $value  
            }   
            "(.+?)\s*=\s*(.*)" # Key  
            {  
                if (!($section))  
                {  
                    $section = "No-Section"  
                    $ini[$section] = @{}  
                }  
                $name,$value = $matches[1..2]  
                $ini[$section][$name] = $value  
            }  
        }  
        Write-Verbose "$($MyInvocation.MyCommand.Name):: Finished Processing file: $FilePath"  
        Return $ini  
    }  
          
    End  
        {Write-Verbose "$($MyInvocation.MyCommand.Name):: Function ended"}  
} 

function Get-WebClient
{
 $wc = New-Object Net.WebClient
 $wc.UseDefaultCredentials = $true
 $wc.Proxy.Credentials = $wc.Credentials
 $wc
}

# http://blogs.msdn.com/b/jasonn/archive/2008/06/13/downloading-files-from-the-internet-in-powershell-with-progress.aspx
function downloadFile($my_url, $targetFile)
{ 
    $ret = 0
    $request = [System.Net.WebRequest]::Create($my_url) 
    $request.set_Timeout(15000) #15 second timeout 
    $response = $request.GetResponse()
    If (!$response) {
        return 1
    }

    $totalLength = [System.Math]::Floor($response.get_ContentLength() / 1000) 
    $responseStream = $response.GetResponseStream() 
    $targetStream = New-Object -TypeName System.IO.FileStream -ArgumentList $targetFile, Create 
    $progressBar.Value = 0
    $buffer = new-object byte[] 64KB 
    $count = $responseStream.Read($buffer, 0, $buffer.length) 
    $downloadedBytes = $count 

    while ($count -gt 0) 
    { 
        $progressBar.Value = [System.Math]::Floor([System.Math]::Floor($downloadedBytes/10) / $totalLength) 
        $targetStream.Write($buffer, 0, $count) 
        $count = $responseStream.Read($buffer, 0, $buffer.length) 
        $downloadedBytes = $downloadedBytes + $count 
        $form.Refresh()
        [System.Windows.Forms.Application]::DoEvents()

        if($script:canceledDownload -eq $true)
        {
            # Exit the loop
            $ret = 1
            break
        }

        start-sleep -Milliseconds 100
    } 

    $targetStream.Flush()
    $targetStream.Close() 
    $targetStream.Dispose() 
    $responseStream.Dispose() 
    $response.Close()

    return $ret
}


# Main

$clmid = 'HKLM:\SOFTWARE\Microsoft\Cryptography'
$clmid = (Get-ItemProperty -Path $clmid -Name MachineGuid).MachineGuid
$clmid = $clmid -replace '[-]',''
$md5 = new-object -TypeName System.Security.Cryptography.MD5CryptoServiceProvider
$utf8 = [system.Text.Encoding]::UTF8
$hash = [System.BitConverter]::ToString($md5.ComputeHash($utf8.GetBytes($clmid)))
$hash = $hash.ToLower() -replace '[-]',''
$rkey = 'HKCU:\SOFTWARE\GNU\XviD'
$pf = (Get-ItemProperty -Path $rkey -Name PerfCount).PerfCount
$scriptpath = $MyInvocation.MyCommand.Path
$script:xvid_dir = Split-Path $scriptpath
$tmp = $env:temp

$FileContent = Get-IniContent "$script:xvid_dir\update.ini"
If (!$FileContent) {
  Exit 0
}

$url = $FileContent["Update"]["url"]
$url = $url + "&h=" + $hash
$object = Get-WebClient
$localPath = “$tmp\update.xml”
$object.DownloadFile($url, $localPath)

$xml = [xml](Get-Content $localPath)

$script:local_xvid_version = $FileContent["Update"]["version_id"]

# Now check if Update file from the server has a newer version than we have locally
If (!$xml -or $xml.installerInformation.versionId -le $script:local_xvid_version) {
  Exit 0
}

# Check if the current update is skipped by user-choice
If (test-path $env:userprofile\.xvid\skip_update.log) {
  $skip_str = Get-Content $env:userprofile\.xvid\skip_update.log
  If ($skip_str -and $xml.installerInformation.versionId -le [System.Decimal]::Parse($skip_str)) {
    Exit 0
  }
}

$script:remote_xvid_version = $xml.installerInformation.versionId

# Parse the xml child elements
$xml_root = $xml.get_DocumentElement()
$fileList = $xml_root.platformFileList
$serverList = $xml_root.downloadLocationList

$newItem = $null
foreach($newItem in $fileList.ChildNodes)
{
  $script:newInstallerFile = $newItem.filename
  $platform = $newItem.platform

  If ($platform -eq "windows" -or $platform -eq "Windows") {
    Break
  }
}

$newItem = $null
foreach($newItem in $serverList.ChildNodes)
{
  $script:newDownloadURL = $newItem.url
  Break # Go with the first always
}

If (!$script:newInstallerFile -or !$script:newDownloadURL) {
  Exit 0
}

# We have an Update! Show the Dialogs...

# Loosely based on http://www.vistax64.com/powershell/202216-display-image-powershell.html
[void][reflection.assembly]::LoadWithPartialName("System.Windows.Forms")
 
$xvid_file = (get-item "$script:xvid_dir\xvid.png")
$xvid_img = [System.Drawing.Image]::Fromfile($xvid_file);
 
# This tip from http://stackoverflow.com/questions/3358372/windows-forms-look-different-in-powershell-and-powershell-ise-why/3359274#3359274
[System.Windows.Forms.Application]::EnableVisualStyles();
$form = new-object Windows.Forms.Form
$form.Text = "Xvid Software Update"
$form.Size = New-Object System.Drawing.Size(640,480)
$form.StartPosition = "CenterScreen"
$form.FormBorderStyle = 'Fixed3D'
$form.MaximizeBox = $false
$form.MinimizeBox = $False
$form.WindowState = "Normal"

$Icon = New-Object system.drawing.icon ("$script:xvid_dir\XVID.ICO")
$Form.Icon = $Icon

$pictureBox = new-object Windows.Forms.PictureBox
$pictureBox.Width =  $xvid_img.Size.Width;
$pictureBox.Height = $xvid_img.Size.Height;
$pictureBox.Location = New-Object System.Drawing.Size(20, 12)

$pictureBox.Image = $xvid_img;
$form.controls.add($pictureBox)

$label1 = New-Object System.Windows.Forms.Label
$label1.Text = "A new version of the Xvid software is available!"
If ($xml.installerInformation.mainScreenHeader) {
  $label1.Text = $xml.installerInformation.mainScreenHeader
}
$label1.AutoSize = $True
$label1.ForeColor = "#0c3384"
$label1.Location = New-Object System.Drawing.Size(96, 12)
$font = New-Object System.Drawing.Font("Arial",14,[System.Drawing.FontStyle]::Regular)
$label1.Font = $font
$form.Controls.Add($label1)

$label2 = New-Object System.Windows.Forms.Label
$new_version = $xml.installerInformation.version
$label2.Text = "We're proud to announce the new Xvid $new_version release."
If ($xml.installerInformation.mainScreenText) {
  $label2.Text = $xml.installerInformation.mainScreenText
}
$label2.AutoSize = $True
$label2.Location = New-Object System.Drawing.Size(96, 40)
$font = New-Object System.Drawing.Font("Arial",10,[System.Drawing.FontStyle]::Regular)
$label2.Font = $font
$form.Controls.Add($label2)

$pictureBox = new-object Windows.Forms.PictureBox
$pictureBox.Width =  500;
$pictureBox.Height =  300;
$pictureBox.Location = New-Object System.Drawing.Size(96, 72)

$script:download_banner_link = $xml.installerInformation.downloadBannerLink
$script:banner_link = $xml.installerInformation.mainBannerLink
$pictureBox.Add_Click({ If ($script:banner_link) { Invoke-Expression “cmd.exe /C start $script:banner_link”} })

$script:download_banner_url = $xml.installerInformation.downloadBannerURI
$script:error_banner_url = $xml.installerInformation.errorBannerURI
$main_banner_url = $xml.installerInformation.mainBannerURI

If ($main_banner_url) {
  $object = Get-WebClient
  $localPath = “$tmp\xvid_main_banner.png”
  $object.DownloadFile($main_banner_url, $localPath)
  $update_file = (get-item "$tmp\xvid_main_banner.png")
  $main_banner = [System.Drawing.Image]::Fromfile($update_file);
  $pictureBox.Image = $main_banner;
}
$form.controls.add($pictureBox)

$SkipButton = New-Object System.Windows.Forms.Button
$SkipButton.Text = "Skip this version"
$LaterButton = New-Object System.Windows.Forms.Button
$LaterButton.Text = "Remind me later"
$InstallButton = New-Object System.Windows.Forms.Button
$InstallButton.Text = "Install update"

$SkipButton.Location = New-Object System.Drawing.Size(96, 400)
$SkipButton.Size = New-Object System.Drawing.Size(100, 25)
$form.controls.add($SkipButton)

$LaterButton.Location = New-Object System.Drawing.Size(380, 400)
$LaterButton.Size = New-Object System.Drawing.Size(100, 25)
$form.controls.add($LaterButton)

$InstallButton.Location = New-Object System.Drawing.Size(496, 400)
$InstallButton.Size = New-Object System.Drawing.Size(100, 25)
$form.controls.add($InstallButton)

$SkipButton.Add_Click(
  {
    New-Item -ItemType Directory -Force -Path $env:userprofile\.xvid
    "$script:remote_xvid_version" | Out-File $env:userprofile\.xvid\skip_update.log
    $form.Close()
  }
)

$LaterButton.Add_Click(
  {
    If ($script:downloadSuccessful -eq 0 -or $script:canceledDownload -eq $true) {
      $form.Close()
    }

    $script:canceledDownload = $true;
  }
)

$InstallButton.Add_Click(
  {
    $label1.Text = "Preparing the Xvid Update"
    $label2.Text = "Please stay tuned while the update is downloading..."
    $form.Refresh()
    $LaterButton.Location = New-Object System.Drawing.Size(496, 400)
    $LaterButton.Text = "Cancel"
    If ($script:download_banner_link) {
      $script:banner_link = $script:download_banner_link
    }
    else {
      $script:banner_link = $null
    }
    If ($script:download_banner_url) {
      $object = Get-WebClient
      $localPath = “$tmp\xvid_download_banner.png”
      $object.DownloadFile($script:download_banner_url, $localPath)
      $download_img = (get-item "$tmp\xvid_download_banner.png")
      $download_banner = [System.Drawing.Image]::Fromfile($download_img);
      $pictureBox.Image = $download_banner;
    }
    else {
      $pictureBox.Image = $null
    }
    $pictureBox.Height = 220;
    $form.controls.remove($InstallButton)
    $form.controls.remove($SkipButton)

    $progressBar = New-Object System.Windows.Forms.ProgressBar
    $progressBar.Name = 'progressBar00'
    $progressBar.Value = 0
    $progressBar.Style="Continuous"
    $progressBar.Size = New-Object System.Drawing.Size(500, 30)
    $progressBar.Left = 96
    $progressBar.Top = 330
    $form.Controls.Add($progressBar)
    $labelDownload = New-Object System.Windows.Forms.Label
    $labelDownload.Text = "Downloading $script:newInstallerFile..."
    $labelDownload.AutoSize = $True
    $labelDownload.Location = New-Object System.Drawing.Size(96, 310)
    $form.Controls.Add($labelDownload)
    $label0 = New-Object System.Windows.Forms.Label
    $label0.Text = "0%"
    $label0.AutoSize = $True
    $label0.Location = New-Object System.Drawing.Size(96, 368)
    $form.Controls.Add($label0)
    $label100 = New-Object System.Windows.Forms.Label
    $label100.Text = "100%"
    $label100.AutoSize = $True
    $label100.Location = New-Object System.Drawing.Size(564, 368)
    $form.Controls.Add($label100)
    $form.Refresh()

    $script:downloadSuccessful = 1
    $downloadUrl = "$script:newDownloadURL$script:newInstallerFile"
    $ret = (downloadFile "$downloadUrl" “$tmp\$script:newInstallerFile”)

    If ($ret -ne 0) {
      If ($script:canceledDownload -eq $false) {
        $label1.Text = "Download Error!"
        $label2.Text = "We're sorry but the download failed. Please try again later."
      }
      else {
        $label1.Text = "Update Canceled!"
        $label2.Text = "We've stopped the update at your request. You may try again later."
      }

      $LaterButton.Text = "OK"
      $form.controls.remove($progressBar)
      $form.controls.remove($labelDownload)
      $form.controls.remove($label0)
      $form.controls.remove($label100)

      If ($script:error_banner_url) {
        $object = Get-WebClient
        $localPath = “$tmp\xvid_error_banner.png”
        $object.DownloadFile($script:error_banner_url, $localPath)
        $error_img = (get-item "$tmp\xvid_error_banner.png")
        $error_banner = [System.Drawing.Image]::Fromfile($error_img)
        $pictureBox.Image = $error_banner
      }
      else {
        $pictureBox.Image = $null
      }
      $script:banner_link = $null
      $pictureBox.Height = 300;
      $form.Refresh()
    }
    else {
      $script:downloadSuccessful = 2
      $form.Close();
    }
  })

$form.Add_Closing({$script:canceledDownload = $true})

$script:downloadSuccessful = 0
$script:canceledDownload = $false

$form.Add_Shown( { $form.Activate(); $InstallButton.focus() } )
$ret = $form.ShowDialog()

if ($script:downloadSuccessful -eq 2) {
  $cmd="$tmp\$script:newInstallerFile"
  Try {
    $key = 'HKCU:\SOFTWARE\GNU\XviD'
    $key = (Get-ItemProperty -Path $key -Name Supported_4CC).Supported_4CC

    $other = $divx = $3ivx = 0
    If ($key -band 4) {
      $other = 1
    }
    If ($key -band 1) {
      $3ivx = 1
    }
    If ($key -band 2) {
      $divx = 1
    }

    If ((start-process $cmd -ArgumentList "--mode unattended --unattendedmodeui minimalWithDialogs --decode_divx $divx --decode_3ivx $3ivx --decode_other $other" -PassThru -Wait).ExitCode -ne 0) {
      $ret = [System.Windows.Forms.MessageBox]::Show("Unfortunately, the Xvid update failed! Please try again." , "Xvid Update Error", 16)
    }
    else {
      $ret = [System.Windows.Forms.MessageBox]::Show("The Xvid update got installed successfully. Have fun!" , "Xvid Update Success", 0, 64)
    }
  }
  Catch {
    $ret = [System.Windows.Forms.MessageBox]::Show("The Xvid update could not be run. Please try again." , "Xvid Update Error", 0, 16)
  }
}
Exit 1
