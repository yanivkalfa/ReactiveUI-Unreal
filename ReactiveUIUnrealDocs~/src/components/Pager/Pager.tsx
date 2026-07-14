import type { FC } from 'react'
import { useMemo } from 'react'
import { useLocation, useNavigate } from 'react-router-dom'
import { Box, Button } from '@mui/material'
import { getFilteredFlat } from '../../docs'
import { useSelectedVersion } from '../../contexts/VersionContext'
import Styles from './Pager.style'

export const Pager: FC = () => {
  const nav = useNavigate()
  const { pathname } = useLocation()
  // Version-filtered like the Sidebar/Search — prev/next must never walk pages the
  // selected engine version hides (latent bug found by the 2026-07-14 audit).
  const { selectedVersion } = useSelectedVersion()
  const flat = useMemo(() => getFilteredFlat(selectedVersion), [selectedVersion])
  const idx = useMemo(() => flat.findIndex((p) => p.path === pathname), [flat, pathname])
  const prev = idx > 0 ? flat[idx - 1] : undefined
  const next = idx >= 0 && idx < flat.length - 1 ? flat[idx + 1] : undefined
  return (
    <Box sx={Styles.root}>
      <span>{prev && <Button onClick={() => nav(prev.path)} variant="text">{'\u2190'} {prev.title}</Button>}</span>
      <span>{next && <Button onClick={() => nav(next.path)} variant="text">{next.title} {'\u2192'}</Button>}</span>
    </Box>
  )
}
