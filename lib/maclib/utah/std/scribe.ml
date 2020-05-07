(defun    
    (apply-look go-forward
	(save-excursion c
	    (if (! (eolp)) (forward-character))
	    (setq go-forward -1)
	    (provide-prefix-argument prefix-argument (backward-word))
	    (setq c (get-tty-character))
	    (if (> c ' ')
		(progn (insert-character '@')
		    (insert-character c)
		    (insert-character '[')
		    (provide-prefix-argument prefix-argument (forward-word))
		    (setq go-forward (dot))
		    (insert-character ']')
		)
		(= c '^Q')
		(progn
		      (setq c (get-tty-no-blanks-input "@" ""))
		      (insert-character '@')
		      (insert-string c)
		      (insert-character '[')
		      (provide-prefix-argument prefix-argument (forward-word))
		      (setq go-forward (dot))
		      (insert-character ']')
		)
	    )
	)
	(if (= go-forward (dot)) (forward-character))
    )
)

(autoload "centre-line" "centre-line.ml")

(defun
    (scribe-mode
	(remove-all-local-bindings)
	(if (! buffer-is-modified)
	    (save-excursion
		(error-occured
		    (goto-character 2000)
		    (search-reverse "LastEditDate=""")
		    (search-forward """")
		    (set-mark)
		    (search-forward """")
		    (backward-character)
		    (delete-to-killbuffer)
		    (insert-string (current-time))
		    (setq buffer-is-modified 0)
		)
	    )
	)
	(local-bind-to-key "justify-paragraph" "\ej")
	(local-bind-to-key "apply-look" "\eq")
	(local-bind-to-key "centre-line" "\es")
	(setq right-margin 77)
	(setq mode-string "Scribe")
	(setq case-fold-search 1)
	(use-syntax-table "text-mode")
	(modify-syntax-entry "w    -'")
	(use-abbrev-table "text-mode")
	(setq left-margin 1)
	(novalue)
    )
)

(novalue)
