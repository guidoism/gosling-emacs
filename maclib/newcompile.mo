��                declare-global  "              �errors-parsed "              �last-command               	 �grep-mode 6                setq    ��               �"  �               defun n             �new-compile-it                �command �             �if  .              �prefix-argument-provided  &             �progn ^  �  ��R              ��arg      2              �": compile-it using command:   �  F�"              �=   �  ��t  �  ��  B�  ��J              ��error-message (              �"No previous command   ,�  ��  ��  �  ��  ��(  �  v�              v"make -k   ��  x�        ��  ��      r             ��save-excursion  @              �pop-to-buffer              	 �"Error-log 6  T�(              �needs-checkpointing       "              T�erase-buffer  *              T�write-modified-files  x  b�J              �>=  *              �process-status    "�      (                kill-process    ��.                start-process   ��  ���  ,�&              �mode-line-format  r              �concat  (              �"        Executing:    0�(              0" (^X^K to kill) %M  ,  ��               �mode-string   ��              ��novalue �                kill-compilation  �  l�*              �temp-use-buffer   ��J              �setq    ��*              �"       Dead!       %M (              �kill-process    �               parse-errors  (              �pop-to-buffer   ���  ��  ��              �progn &              �beginning-of-file �              �error-occured t              �re-replace-string ,              �"^\([^:]*\):\([0-9]*\):  "              �"\1, line \2:  6  f�(              �buffer-is-modified          �              �set-mark                 �end-of-file 4              �parse-error-messages-in-region    ��  <�                   
 ��next-error  "               new-next-error  �  ��  ��  ��       ���  ��*              �<   `�  ��        ���  ���  D�t              �get-tty-string  P             : �"The compilation is still running, do you want to kill it?                 �"y   <�  �  ��j                bind-to-key $              �"new-compile-it  &              �"+           "  ��  r�  ��          "  x�  z�  ��                          novalue 