�               progn l              �setq  &              �mode-line-format  ,              �"Loading compare windows P             �defun 6             �compare-windows               �dot1                �dot2                �str1                �str2                �match �             �save-excursion    ��  ��     |             ��while $              �>   ��                    �set-mark  �              �if                �eobp    �  �  ����~  ��               �end-of-line &              �forward-character 2  ��  ��&              ��region-to-string  &  ��  $�              $�mark                   next-window   ���  ��  �R  ��$              �=   �  ����  ��  
�  ����  ��  ��  ����$  ��  ��  ��  ��  ��  �  ��  p�  6�X  `�  �  ��      >  F�$              �!=    F�  Z�  \�  h�      $                previous-window � ��   �  *�      �  ��*              �goto-character    ��  ��  ��  ��  ��:              ��message               �"Mismatch    j�  ��  �����  Z�               �end-of-file   ��  H�  �  ��n  x�h              �concat  $              �"End of buffer   (              �"current-buffer-name   ��  ��  ����<  ��  ��  R�  L�  2�  ��  d�  z�  ��  :�0  ��*              �"No differences found  :  H�  \�.              \�default-mode-line-format  