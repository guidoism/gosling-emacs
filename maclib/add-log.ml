(defun
    (add-change-log-entry
	(visit-file "ChangeLog")
	(setq mode-string "log")
	(local-bind-to-key "justify-paragraph" (+ 128 'j'))
	(beginning-of-file)
	(insert-string (current-time))
	(insert-string "  ")
	(insert-string (users-full-name))
	(insert-string "  (")
	(insert-string (users-login-name))
	(insert-string " at ")
	(insert-string (system-name))
	(insert-string ")")
	(newline)
	(setq left-margin 9)
	(setq right-margin 75)
	(to-col left-margin)
	(newline-and-backup)
	(newline-and-backup)
    )
)
