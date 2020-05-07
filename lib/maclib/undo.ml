(defun
    (new-undo c
	(undo)
	(while (progn (message "Hit <space> to undo more; <cr> to stop")
		   (= (setq c (get-tty-character)) ' '))
	    (undo-boundary)
	    (undo-more))
	(message "Finished undoing.")
	(if (& (!= c '\n')
		(!= c '\r'))
	    (push-back-character c))
    )
)
