�4             �defun �            	 �find-line "              �char-per-line                �line-number              	 �increment (              �setq    ��  (   V             �save-excursion               	 �start-dot              	 �prev-incr              	 �curr-incr &              �beginning-of-line $  8�  ~�              ~�dot &                beginning-of-file   ��  ��       ��  >�      .  ��  ��"              ��/   ��  >�               while $              �!=    >�  ��                set-mark  4               if  "              �<   ��  T�2               progn P              �provide-prefix-argument   ��             	 ��next-line   ��  �  n��  N�D              �&               �eobp    ��  ,�     >                goto-character                �mark  .  ��  ��"              ��+   ��  ���  ��.  ��  ��"              ��previous-line 0  ��  �$              �-         T�d  4�4  ��              �bobp    ��  "�       ��  �  &�  ��  ��  ��  ��f  ��  ��  ��  ��  ��  ��  .�  ��  ��0  ��  l�$              l�*        H�*  ��  ~�  ��  �  ��  \�  �J  @�$              �>         6�   N�  *�  ��        �H  ��"              �=   ��  v�   �  ��  n�  ��        ��  J�  N�        T�@  ��&              �>=          ��  ��  v�     �  N�               �interactive d              �message H              �concat  &              �"Current line is     ��  ���             	   goto-line   ���  ��2  ��$              �prefix-argument      N  ��  V�B              V�arg      "              �": goto-line     L�  �  ~�  *�:  ��  ��  ��        6�  ��  ��       N�D               goto-percentage-of-file               �pof               �ppof                �input               �done    h�  ��      "  T�  ��               �"    2�  ��      ��  ��  ��     L  ��F  ��              �"go to     *�             	 *"% of file 2  ��  �&              �get-tty-character � N�J  �  V�  ��     0  ��"              �length    ��      v  8�P  0�  x�D              x�substr    V�       n�  ��  <�        ��  �  �  �  
     ��  "�     @              ��error-message               �"Aborted.    H�  ��     ,  Z�  R�  ��  �  @�  p�      R                |   ��  x�       ��  d�       ��  P�  
     ��  V�      @  J�  �  "�  0   &              �<=    ��  9   �  ��B  ��  ��6  4�  ��*              ��char-to-string    ��:  >�  n�.  D�  j�  \�  
     ��  z�  0   N  ��H  H�:  6�               �buffer-size   ��  �       d   N               Count-Lines-In-Region               �count 
             �save-restriction  "              �narrow-region                �end-of-file   
�  |�  ��  ��  v�~  ��\  ��V  ��              
 �"There are     2�*              2" lines in the region                  novalue   ��